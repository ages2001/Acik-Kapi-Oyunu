// Report the DOS image entry points and decoded callers of its published memory options.
// @category WINRAM
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.scalar.Scalar;
import java.io.*;
import java.util.*;
import java.nio.charset.StandardCharsets;
public class PatchmemInventory extends GhidraScript {
    public void run() throws Exception {
        Memory mem=currentProgram.getMemory();
        Listing list=currentProgram.getListing();
        FunctionManager fm=currentProgram.getFunctionManager();
        try(PrintWriter out=new PrintWriter(getScriptArgs()[0])) {
            out.println("Image "+currentProgram.getName()+" in "+currentProgram.getLanguageID());
            out.println("Recovered functions "+fm.getFunctionCount());
            for (String s : new String[]{"Move AGP","Absolute Max File Cache","Install Only","Move VXD Data","Install PROTHOOK","VMM File Extracted","VCACHE File Extracted","Unknown, Invalid or Uncompressed VMM32 File","VMM32 Patched","VCACHE Patched"}) {
                Address p=mem.findBytes(currentProgram.getMinAddress(),s.getBytes(StandardCharsets.US_ASCII),null,true,monitor);
                if (p==null)continue;
                out.println(s+"  "+p);
            }
            for(Function f:fm.getFunctions(true)){
                int calls=0;
                for(Instruction i:list.getInstructions(f.getBody(),true))if(i.getFlowType().isCall())calls++;
                if(calls>20)out.println("Dispatcher candidate "+f.getEntryPoint()+" body "+f.getBody().getNumAddresses()+" decoded calls "+calls);
            }
        }
    }
}
