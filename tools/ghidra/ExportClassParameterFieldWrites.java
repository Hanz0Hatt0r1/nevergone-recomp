import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.DataType;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.pcode.HighFunction;
import ghidra.program.model.pcode.HighSymbol;
import ghidra.program.model.pcode.HighVariable;
import ghidra.program.model.pcode.LocalSymbolMap;
import ghidra.program.model.pcode.PcodeOp;
import ghidra.program.model.pcode.PcodeOpAST;
import ghidra.program.model.pcode.SequenceNumber;
import ghidra.program.model.pcode.Varnode;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Set;

// Evidence-only exporter for writes to parameter-relative fields across every
// Ghidra function owned by one exact C++ class name. It does not infer the field
// class. Stored values are classified only by exact high-P-code provenance:
// direct CALL root, function parameter root, constant, or another opcode.
public class ExportClassParameterFieldWrites extends GhidraScript {
    private static final String FORMAT = "NEVERGONE.GhidraClassParameterFieldWrite/1";
    private static final int MAX_DEPTH = 20;
    private static final int MAX_TRANSPARENT = 10;

    private static class Expr {
        final Varnode base;
        final Long offset;
        final String kind;
        Expr(Varnode base, Long offset, String kind) { this.base=base; this.offset=offset; this.kind=kind; }
    }

    private String cell(Object value) {
        if (value == null) return "-";
        return String.valueOf(value).replace('\t',' ').replace('\n',' ').replace('\r',' ');
    }

    private String token(Varnode node) {
        if (node == null) return "-";
        if (node.isConstant()) return "const:0x" + Long.toUnsignedString(node.getOffset(),16) + ":" + node.getSize();
        PcodeOp def=node.getDef();
        if (def != null) {
            SequenceNumber seq=def.getSeqnum();
            return "def:" + def.getMnemonic() + "@" + cell(seq.getTarget()) + ":" + seq.getTime() + ":" + node.getSize();
        }
        String kind=node.isRegister()?"register":node.isUnique()?"unique":node.isAddress()?"address":"input";
        return kind + ":" + cell(node.getAddress()) + ":" + node.getSize();
    }

    private boolean transparent(PcodeOp op) {
        if (op == null) return false;
        int c=op.getOpcode();
        return c==PcodeOp.COPY || c==PcodeOp.CAST || c==PcodeOp.INDIRECT;
    }

    private Varnode strip(Varnode original) {
        Varnode cur=original;
        HashSet<Varnode> seen=new HashSet<Varnode>();
        for (int d=0; cur!=null && d<MAX_TRANSPARENT && seen.add(cur); d++) {
            PcodeOp def=cur.getDef();
            if (!transparent(def) || def.getNumInputs()<1) break;
            cur=def.getInput(0);
        }
        return cur;
    }

    private Expr decode(Varnode node, int depth, Set<Varnode> active) {
        if (node==null || depth>MAX_DEPTH || active.contains(node)) return new Expr(null,null,"missing_or_cycle");
        if (node.isConstant()) return new Expr(null,node.getOffset(),"constant");
        PcodeOp def=node.getDef();
        if (def==null) return new Expr(node,0L,"base");
        Set<Varnode> next=new HashSet<Varnode>(active); next.add(node);
        if (transparent(def) && def.getNumInputs()>=1) return decode(def.getInput(0),depth+1,next);
        int c=def.getOpcode();
        if (c==PcodeOp.MULTIEQUAL) return new Expr(null,null,"phi_ambiguous");
        try {
            if (def.getNumInputs()>=2 && (c==PcodeOp.PTRSUB || c==PcodeOp.INT_ADD || c==PcodeOp.INT_SUB || c==PcodeOp.PTRADD)) {
                Varnode a=def.getInput(0), b=def.getInput(1), base=a;
                Long delta=null;
                if (b!=null && b.isConstant()) {
                    delta=b.getOffset();
                    if (c==PcodeOp.INT_SUB) delta=Math.negateExact(delta);
                    if (c==PcodeOp.PTRADD) {
                        if (def.getNumInputs()<3 || !def.getInput(2).isConstant()) return new Expr(null,null,"ptradd_unknown_scale");
                        delta=Math.multiplyExact(delta,def.getInput(2).getOffset());
                    }
                } else if (c==PcodeOp.INT_ADD && a!=null && a.isConstant()) {
                    delta=a.getOffset(); base=b;
                }
                if (delta!=null) {
                    Expr child=decode(base,depth+1,next);
                    if (child.offset==null) return new Expr(null,null,"unresolved_arithmetic_base");
                    return new Expr(child.base,Math.addExact(child.offset,delta),"base_plus_constant");
                }
            }
        } catch (ArithmeticException ignored) { return new Expr(null,null,"offset_overflow"); }
        return new Expr(node,0L,"opaque_"+def.getMnemonic());
    }

    private int parameterIndex(HighFunction high, Varnode node) {
        if (high==null || node==null || node.getHigh()==null) return -1;
        LocalSymbolMap map=high.getLocalSymbolMap();
        if (map==null) return -1;
        HighVariable candidate=node.getHigh();
        for (int i=0;i<map.getNumParams();i++) {
            HighSymbol sym=map.getParamSymbol(i);
            if (sym==null || sym.getHighVariable()==null) continue;
            HighVariable p=sym.getHighVariable();
            if (candidate==p) return i;
            if (candidate.getRepresentative()!=null && p.getRepresentative()!=null && candidate.getRepresentative().equals(p.getRepresentative())) return i;
        }
        return -1;
    }

    private String parameterType(HighFunction high, int index) {
        if (high==null || index<0) return "-";
        LocalSymbolMap map=high.getLocalSymbolMap();
        if (map==null || index>=map.getNumParams()) return "-";
        HighSymbol sym=map.getParamSymbol(index);
        if (sym==null || sym.getHighVariable()==null || sym.getHighVariable().getDataType()==null) return "-";
        return cell(sym.getHighVariable().getDataType().getDisplayName());
    }

    private String highType(Varnode node) {
        if (node==null || node.getHigh()==null) return "-";
        DataType type=node.getHigh().getDataType();
        return type==null?"-":cell(type.getDisplayName());
    }

    private Function directTarget(PcodeOp op) {
        if (op==null || op.getOpcode()!=PcodeOp.CALL || op.getNumInputs()<1) return null;
        Varnode target=strip(op.getInput(0));
        if (target==null) return null;
        Address address=null;
        try {
            if (target.isAddress()) address=target.getAddress();
            else if (target.isConstant()) address=toAddr(Long.toHexString(target.getOffset()));
        } catch (Exception ignored) {}
        if (address==null) return null;
        Function f=currentProgram.getFunctionManager().getFunctionAt(address);
        if (f==null && (address.getOffset()&1L)!=0L) {
            try { f=currentProgram.getFunctionManager().getFunctionAt(address.getAddressSpace().getAddress(address.getOffset()&~1L)); }
            catch (Exception ignored) {}
        }
        if (f==null) f=currentProgram.getFunctionManager().getFunctionContaining(address);
        return f;
    }

    private String hex(Long value) {
        if (value==null) return "-";
        if (value.longValue()<0) return "-0x" + Long.toUnsignedString(-value.longValue(),16);
        return "0x" + Long.toUnsignedString(value.longValue(),16);
    }

    private boolean ownedBy(String className, Function function) {
        String prefix=className+"::";
        String full=function.getName(true);
        String shortName=function.getName();
        return (full!=null && full.startsWith(prefix)) || (shortName!=null && shortName.startsWith(prefix));
    }

    public void run() throws Exception {
        String[] args=getScriptArgs();
        if (args.length!=2) throw new IllegalArgumentException("Usage: ExportClassParameterFieldWrites.java <exact-class-name> <output.tsv>");
        String className=args[0];
        File output=new File(args[1]); if (output.getParentFile()!=null) output.getParentFile().mkdirs();
        List<Function> functions=new ArrayList<Function>();
        FunctionIterator fit=currentProgram.getFunctionManager().getFunctions(true);
        while (fit.hasNext() && !monitor.isCancelled()) { Function f=fit.next(); if (ownedBy(className,f)) functions.add(f); }
        if (functions.isEmpty()) throw new IllegalArgumentException("No functions found for exact class: "+className);

        DecompInterface decompiler=new DecompInterface();
        if (!decompiler.openProgram(currentProgram)) throw new IllegalStateException("Could not open program");
        int decompiled=0, failed=0, stores=0, param0Stores=0;
        try (BufferedWriter out=new BufferedWriter(new OutputStreamWriter(new FileOutputStream(output),StandardCharsets.UTF_8))) {
            out.write("format\tclass_name\tfunction_entry\tfunction_name\top_address\top_time\taddress_kind\taddress_base_token\taddress_offset\tbase_parameter_index\tvalue_token\tvalue_root_token\tvalue_root_opcode\tvalue_root_type_display\tvalue_root_parameter_index\tvalue_root_parameter_type_display\tfactory_target_entry\tfactory_target_name");
            out.newLine();
            for (Function function:functions) {
                if (monitor.isCancelled()) break;
                DecompileResults results=decompiler.decompileFunction(function,60,monitor);
                if (results==null || !results.decompileCompleted() || results.getHighFunction()==null) { failed++; continue; }
                decompiled++;
                HighFunction high=results.getHighFunction();
                Iterator<PcodeOpAST> it=high.getPcodeOps();
                while (it.hasNext()) {
                    PcodeOpAST op=it.next();
                    if (op.getOpcode()!=PcodeOp.STORE || op.getNumInputs()<3) continue;
                    stores++;
                    Expr expr=decode(op.getInput(1),0,new HashSet<Varnode>());
                    int baseParam=parameterIndex(high,expr.base);
                    if (baseParam!=0) continue;
                    param0Stores++;
                    Varnode value=op.getInput(2); Varnode root=strip(value); PcodeOp rootDef=root==null?null:root.getDef();
                    int rootParam=parameterIndex(high,root); Function target=directTarget(rootDef); SequenceNumber seq=op.getSeqnum();
                    out.write(String.join("\t",
                        FORMAT,cell(className),cell(function.getEntryPoint()),cell(function.getName(true)),
                        cell(seq==null?null:seq.getTarget()),cell(seq==null?null:seq.getTime()),
                        cell(expr.kind),token(expr.base),hex(expr.offset),Integer.toString(baseParam),
                        token(value),token(root),cell(rootDef==null?null:rootDef.getMnemonic()),highType(root),
                        rootParam<0?"-":Integer.toString(rootParam),parameterType(high,rootParam),
                        cell(target==null?null:target.getEntryPoint()),cell(target==null?null:target.getName(true))));
                    out.newLine();
                }
            }
        } finally { decompiler.dispose(); }
        println("CLASS_PARAMETER_FIELD_WRITE_FORMAT="+FORMAT);
        println("CLASS_PARAMETER_FIELD_WRITE_CLASS="+className);
        println("CLASS_PARAMETER_FIELD_WRITE_FUNCTIONS="+functions.size());
        println("CLASS_PARAMETER_FIELD_WRITE_DECOMPILED="+decompiled);
        println("CLASS_PARAMETER_FIELD_WRITE_FAILED="+failed);
        println("CLASS_PARAMETER_FIELD_WRITE_ALL_STORES="+stores);
        println("CLASS_PARAMETER_FIELD_WRITE_PARAM0_STORES="+param0Stores);
        println("CLASS_PARAMETER_FIELD_WRITE_OUTPUT="+output.getAbsolutePath());
        println("CLASS_PARAMETER_FIELD_WRITE_AFFECTS_ROUTING=no");
    }
}
