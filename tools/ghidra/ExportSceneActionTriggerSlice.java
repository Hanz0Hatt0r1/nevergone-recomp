// Export only the narrow SceneActions spatial-trigger slice needed for clean-room recovery.
// @category Nevergone

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

import java.io.File;
import java.io.PrintWriter;
import java.util.LinkedHashMap;
import java.util.Map;

public class ExportSceneActionTriggerSlice extends GhidraScript {
    private static final class Target {
        final String label;
        final String address;

        Target(String label, String address) {
            this.label = label;
            this.address = address;
        }
    }

    private static final Target[] TARGETS = {
        new Target("GameScene::TriggerSceneAction(float)", "00344934"),
        new Target("GameSceneClassLayer::showEventObjTip(...) ", "0034b048"),
        new Target("GameSceneObject::triggerSceneAction()", "0034e1b8"),
        new Target("GameSceneObject::isContainsRect(CCRect)", "0034e268"),
        new Target("GameSceneObject::isContainsRect(CCRect,const char*)", "0034e454"),
        new Target("GameSceneObject::getObjectRect()", "0034e4a6"),
    };

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1 || args[0].trim().isEmpty()) {
            throw new IllegalArgumentException(
                "Usage: ExportSceneActionTriggerSlice.java <output-file>");
        }

        File output = new File(args[0]).getCanonicalFile();
        File parent = output.getParentFile();
        if (parent != null && !parent.exists() && !parent.mkdirs()) {
            throw new IllegalStateException("Could not create output directory: " + parent);
        }

        DecompInterface decompiler = new DecompInterface();
        decompiler.toggleCCode(true);
        decompiler.toggleSyntaxTree(true);
        if (!decompiler.openProgram(currentProgram)) {
            throw new IllegalStateException("Could not open current program in decompiler");
        }

        try (PrintWriter out = new PrintWriter(output, "UTF-8")) {
            out.println("# Nevergone SceneActions spatial-trigger slice");
            out.println("program=" + currentProgram.getName());
            out.println("imageBase=" + currentProgram.getImageBase());
            out.println("language=" + currentProgram.getLanguageID());
            out.println();

            for (Target target : TARGETS) {
                monitor.checkCancelled();
                exportTarget(out, decompiler, target);
            }
        } finally {
            decompiler.dispose();
        }

        println("Wrote SceneActions trigger slice to: " + output.getAbsolutePath());
    }

    private void exportTarget(PrintWriter out, DecompInterface decompiler, Target target)
            throws Exception {
        Address address = currentProgram.getAddressFactory()
            .getDefaultAddressSpace()
            .getAddress(target.address);
        Function function = getFunctionAt(address);
        if (function == null) function = getFunctionContaining(address);

        out.println("============================================================");
        out.println("target=" + target.label);
        out.println("requestedAddress=0x" + target.address);
        if (function == null) {
            out.println("status=FUNCTION_NOT_FOUND");
            out.println();
            return;
        }

        out.println("function=" + function.getName(true));
        out.println("entry=" + function.getEntryPoint());
        out.println("prototype=" + function.getPrototypeString(true, true));
        out.println("body=" + function.getBody());
        out.println();

        out.println("-- CALLERS --");
        Map<String, String> callers = new LinkedHashMap<>();
        ReferenceIterator refs = currentProgram.getReferenceManager()
            .getReferencesTo(function.getEntryPoint());
        while (refs.hasNext()) {
            monitor.checkCancelled();
            Reference ref = refs.next();
            Function caller = getFunctionContaining(ref.getFromAddress());
            String key = ref.getFromAddress().toString();
            String value = caller == null
                ? "<no-function>"
                : caller.getName(true) + " @ " + caller.getEntryPoint();
            callers.put(key, value);
        }
        if (callers.isEmpty()) out.println("<none>");
        for (Map.Entry<String, String> caller : callers.entrySet()) {
            out.println(caller.getKey() + " -> " + caller.getValue());
        }
        out.println();

        out.println("-- INSTRUCTIONS --");
        InstructionIterator instructions = currentProgram.getListing()
            .getInstructions(function.getBody(), true);
        while (instructions.hasNext()) {
            monitor.checkCancelled();
            Instruction instruction = instructions.next();
            out.println(instruction.getAddress() + "\t" + instruction);
        }
        out.println();

        out.println("-- DECOMPILE --");
        DecompileResults result = decompiler.decompileFunction(function, 60, monitor);
        if (!result.decompileCompleted() || result.getDecompiledFunction() == null) {
            out.println("DECOMPILE_FAILED: " + result.getErrorMessage());
        } else {
            out.println(result.getDecompiledFunction().getC());
        }
        out.println();
    }
}
