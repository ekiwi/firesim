package coverage.midas

import chisel3._
import chisel3.util._
import chisel3.experimental.{DataMirror, Direction}

import scala.collection.immutable.ListMap
import freechips.rocketchip.config.Parameters
import freechips.rocketchip.util.DecoupledHelper
import midas.widgets._


case class CoverageBridgeKey(counterWidth: Int, covers: List[String])


class CoverageBundle(val counterWidth: Int) extends Bundle {
  val clock = Input(Clock())
  val cover_en = Output(Bool())
  val cover_out = Input(UInt(counterWidth.W))
}

class CoverageBridgeModule(key: CoverageBridgeKey)(implicit p: Parameters) extends BridgeModule[HostPortIO[CoverageBundle]]()(p) {
  lazy val module = new BridgeModuleImp(this) {
    val io = IO(new WidgetIO())
    val hPort = IO(HostPort(new CoverageBundle(key.counterWidth)))

    // keep scan chain disabled for now
    hPort.toHost.hReady := true.B

    // ignore scanchain output for now
    hPort.fromHost.hValid := true.B
    hPort.hBits.cover_en := false.B

    // count simulation cycles (this is just to learn more about bridges)
    val simCycles = RegInit(0.U(64.W))
    when(hPort.toHost.fire()) { simCycles := simCycles + 1.U }
    genROReg(simCycles(31,0), "sim_cycles")
    genROReg(simCycles(63,32), "sim_cycles_2")


    // we are going to clock out the coverage after N cycles
    val CoverageAt = 4000
    // the cover counter counts how long we enable to scan chain for
    val coverCounter = RegInit(0.U(32.W))
    when(hPort.fromHost.fire() && hPort.hBits.cover_en) {
      coverCounter := coverCounter + 1.U
    }
    val scanning = RegInit(false.B)
    genRORegInit(scanning, "scanning", false.B)

    // the register is used to start scanning
    val startScanning = Wire(Bool())
    Pulsify(genWORegInit(startScanning, "start_scanning", false.B), 1)
    when(startScanning) { scanning := true.B; coverCounter := 0.U }


    when(scanning) {
      // in scanning mode we keep the scan chain enabled until we have scanned out everything
      hPort.hBits.cover_en := true.B
      when(coverCounter === key.covers.length.U) {
        scanning := false.B
      }
    }

    // use a queue that is big enough to contain all cover counts
    val nextPowerOfTwo = BigInt(1) << log2Ceil(key.covers.length)
    val counterType = UInt(key.counterWidth.W)
    val counts = Module(new Queue(counterType, nextPowerOfTwo.toInt))
    counts.io.enq.bits := hPort.hBits.cover_out
    counts.io.enq.valid := scanning && hPort.toHost.fire()
    // we don't empty the queue yet :(
    counts.io.deq.ready := false.B

    genROReg(counts.io.count, "cover_data_available")


    val test = 1234567.U(32.W)
    genROReg(test, "cover_test")

    genCRFile()

    override def genHeader(base: BigInt, sb: StringBuilder) {
      import CppGenerationUtils._
      val headerWidgetName = getWName.toUpperCase
      super.genHeader(base, sb)
      sb.append(genConstStatic(s"${headerWidgetName}_cover_count", UInt32(key.covers.length)))
      sb.append(genConstStatic(s"${headerWidgetName}_counter_width", UInt32(key.counterWidth)))
      sb.append(genArray(s"${headerWidgetName}_covers", key.covers.map(CStrLit)))
    }
  }
}
