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

    // keep scan chain disables for now
    hPort.toHost.hReady := true.B

    // ignore scanchain output for now
    hPort.fromHost.hValid := true.B
    hPort.hBits.cover_en := false.B

    // count simulation cycles
    val simCycles = RegInit(0.U(64.W))
    when(hPort.toHost.fire()) { simCycles := simCycles + 1.U }
    genROReg(simCycles, "sim_cycles")

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
