package coverage.midas

import chisel3._
import chisel3.util._
import freechips.rocketchip.config.Parameters
import midas.widgets._

case class CoverageBridgeKey(counterWidth: Int, coverCount: Int)

class CoverageBundle(val counterWidth: Int) extends Bundle {
  val en = Output(Bool())
  val out = Input(UInt(counterWidth.W))
}

class CoverageBridgeModule(key: CoverageBridgeKey)(implicit p: Parameters) extends BridgeModule[HostPortIO[CoverageBundle]]()(p) {
  lazy val module = new BridgeModuleImp(this) {
    val io = IO(new WidgetIO())
    val hPort = IO(HostPort(new CoverageBundle(key.counterWidth)))

    // keep scan chain disables for now
    hPort.toHost.hValid := true.B
    hPort.hBits.en := false.B

    // ignore scanchain output for now
    hPort.fromHost.hReady := true.B

    val test = 1234567.U(32.W)
    genROReg(test, "cover_test")

    genCRFile()

    override def genHeader(base: BigInt, sb: StringBuilder) {
      val headerWidgetName = getWName.toUpperCase
      super.genHeader(base, sb)
      sb.append(genConstStatic(s"${headerWidgetName}_cover_count", UInt32(key.coverCount)))
      sb.append(genConstStatic(s"${headerWidgetName}_counter_width", UInt32(key.counterWidth)))
    }
  }
}
