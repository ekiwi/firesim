package coverage.midas

import chisel3._
import chisel3.util._
import chisel3.experimental.{DataMirror, Direction}

import scala.collection.immutable.ListMap
import freechips.rocketchip.config.Parameters
import freechips.rocketchip.util.DecoupledHelper
import midas.widgets._


case class CoverageBridgeKey(counterWidth: Int, covers: List[String]) {
  override def toString =
    s"CoverageBridgeKey(counterWidth = $counterWidth, covers = ${covers.length})"
}


class CoverageBundle(val counterWidth: Int) extends Bundle {
  val clock = Input(Clock())
  val cover_en = Output(Bool())
  val cover_out = Input(UInt(counterWidth.W))
}

class CoverageBridgeModule(key: CoverageBridgeKey)(implicit p: Parameters) extends BridgeModule[HostPortIO[CoverageBundle]]()(p) {
  lazy val module = new BridgeModuleImp(this) with UnidirectionalDMAToHostCPU {
    val io = IO(new WidgetIO())
    val hPort = IO(HostPort(new CoverageBundle(key.counterWidth)))

    // derive some basic constants from the key
    val CounterWidth = key.counterWidth
    val PowerOfTwoCounterWidth = 1 << log2Ceil(CounterWidth)
    val NumberOfCounters = key.covers.length

    // remember whether we are in scanning mode
    val scanning = RegInit(false.B)
    genRORegInit(scanning, "scanning", false.B)

    // register to start scanning
    val startScanning = Wire(Bool())
    Pulsify(genWORegInit(startScanning, "start_scanning", false.B), 1)

    //////////////////////////////////////////////////////////////////
    // fromTarget: we connect the cover chain output to the DMA queue
    hPort.toHost.hReady := true.B

    // DMA port
    override lazy val toHostCPUQueueDepth = 6144 // 12 Ultrascale+ URAMs
    override lazy val dmaSize = BigInt(dmaBytes * toHostCPUQueueDepth)

    // how many coverage counters fit into a single DMA beat?
    val CountersPerBeat = (dmaBytes * 8) / PowerOfTwoCounterWidth
    require(CountersPerBeat > 0,
      f"Coverage counter size (${key.counterWidth}bit) cannot be greater than the DMA beat size (${dmaBytes * 8}bit)")

    // adapt smaller counter tokens to larger DMA size
    val widthAdapter = Module(new NarrowToWideAdapter(PowerOfTwoCounterWidth, dma.nastiXDataBits))
    outgoingPCISdat.io.enq <> widthAdapter.io.out

    // receiving data is delayed by two _target_ cycles because of the
    // registers on the from/to host interface
    val receiving = RegEnable(RegEnable(scanning, false.B, hPort.toHost.fire()), false.B, hPort.toHost.fire())

    // receive data from target
    widthAdapter.io.in.bits := hPort.hBits.cover_out
    widthAdapter.io.in.valid := receiving && hPort.toHost.fire()

    // we want to potentially receive some more values in order to flush out the size adapter
    val ValuesToRead = math.ceil(NumberOfCounters.toDouble / CountersPerBeat.toDouble).toInt * CountersPerBeat

    // count the number of values we have received while in scanning mode
    val coverOutCounter = Reg(UInt(32.W))
    val fromHostDone = coverOutCounter === ValuesToRead.U
    when(startScanning) { coverOutCounter := 0.U }
    .elsewhen(hPort.toHost.fire() && receiving && !fromHostDone) {
      coverOutCounter := coverOutCounter + 1.U
    }

    //////////////////////////////////////////////////////////////////
    // toTarget

    // we count the number of _simulation_ cycles that the cover chain is active in
    val enCount = Reg(UInt(32.W))
    val toHostDone = enCount === key.covers.length.U
    when(startScanning) { enCount := 0.U }
        .elsewhen(hPort.fromHost.fire() && scanning && !toHostDone) {
          enCount := enCount + 1.U
        }

    // the scan chain is enabled iff we are in scanning mode
    hPort.fromHost.hValid := true.B
    hPort.hBits.cover_en := scanning && !toHostDone

    // logic to start/stop scanning
    when(startScanning) { scanning := true.B }
    when(scanning && fromHostDone && toHostDone) { scanning := false.B }

    genCRFile()

    override def genHeader(base: BigInt, sb: StringBuilder) {
      import CppGenerationUtils._
      val headerWidgetName = getWName.toUpperCase
      super.genHeader(base, sb)
      sb.append(genConstStatic(s"${headerWidgetName}_cover_count", UInt32(NumberOfCounters)))
      sb.append(genConstStatic(s"${headerWidgetName}_counter_width", UInt32(PowerOfTwoCounterWidth)))
      sb.append(genConstStatic(s"${headerWidgetName}_counters_per_beat", UInt32(CountersPerBeat)))
      // we reverse the covers in order to have the name of the first counter to be read at the head
      sb.append(genArray(s"${headerWidgetName}_covers", key.covers.reverse.map(CStrLit)))
    }

  }
}


class NarrowToWideAdapter(inW: Int, outW: Int)  extends MultiIOModule {
  require(inW < outW, s"This module requires the input with to be smaller than the output width!")
  require(inW > 0, s"This module requires the input to be at least 1-bit wide!")
  require(outW % inW == 0, s"NarrowToWideAdapter: out: $outW not divisible by in: $inW")

  val io = IO(new Bundle {
    val in = Flipped(Decoupled(UInt(inW.W)))
    val out = Decoupled(UInt(outW.W))
  })

  val nBeats = outW / inW

  // shift new values in whenever a valid input transaction happens
  val doShift = io.in.fire()
  // TODO: replace with ShiftRegisters when Chisel 3.5 is released
  val regs = Seq.iterate(io.in.bits, nBeats + 1)(RegEnable(_, doShift)).drop(1)
  val inputBuffer = RegEnable(io.in.bits, doShift)
  val msb = Mux(doShift, io.in.bits, inputBuffer)
  io.out.bits := Cat(Seq(msb) ++ regs)

  // count the number of (valid) entries in the shift register
  val count = RegInit(0.U(log2Ceil(nBeats+1).W))
  when(io.out.fire()) { count := doShift }
  .elsewhen(doShift) { count := count + 1.U }
  val shiftFull = count === nBeats.U

  // we can accept new data if the shift register is not full, or if it will be emptied this cycle
  io.in.ready := !shiftFull || io.out.fire()

  // we can output data when the shift register is full
  io.out.valid := shiftFull
}
