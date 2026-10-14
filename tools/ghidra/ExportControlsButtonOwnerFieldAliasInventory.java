import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.pcode.HighFunction;
import ghidra.program.model.pcode.HighSymbol;
import ghidra.program.model.pcode.HighVariable;
import ghidra.program.model.pcode.LocalSymbolMap;
import ghidra.program.model.pcode.PcodeOp;
import ghidra.program.model.pcode.PcodeOpAST;
import ghidra.program.model.pcode.SequenceNumber;
import ghidra.program.model.pcode.Varnode;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Set;

// Read-only bounded inventory of every high-P-code STORE/CALL/CALLIND in the
// exact 19 ControlsButton method entrypoints emitted by the SHA-pinned native
// planner. Parameter-zero roots are reported with recovered constant offsets;
// unresolved expressions remain explicit and never imply class/field proof.
public class ExportControlsButtonOwnerFieldAliasInventory extends GhidraScript {
    private static final String FORMAT = "NEVERGONE.GhidraControlsButtonOwnerFieldAliasInventory/1";
    private static final int MAX_DEPTH = 20;

    private static class Expr {
        final Varnode root;
        final Long offset;
        final String kind;
        Expr(Varnode root, Long offset, String kind) { this.root=root; this.offset=offset; this.kind=kind; }
    }

    private String cell(Object value) {
        if (value == null) return "-";
        return String.valueOf(value).replace('\t',' ').replace('\r',' ').replace('\n',' ');
    }

    private String token(Varnode node) {
        if (node == null) return "-";
        if (node.isConstant()) return "const:0x" + Long.toUnsignedString(node.getOffset(),16) + ":" + node.getSize();
        PcodeOp def=node.getDef();
        if (def != null) {
            SequenceNumber seq=def.getSeqnum();
            return "def:"+def.getMnemonic()+"@"+cell(seq.getTarget())+":"+seq.getTime()+":"+node.getSize();
        }
        String kind=node.isRegister()?"register":node.isUnique()?"unique":node.isAddress()?"address":"input";
        return kind+":"+cell(node.getAddress())+":"+node.getSize();
    }

    private boolean transparent(PcodeOp op) {
        if (op==null) return false;
        int c=op.getOpcode();
        return c==PcodeOp.COPY || c==PcodeOp.CAST || c==PcodeOp.INDIRECT;
    }

    private Expr trace(Varnode node, int depth, Set<Varnode> seen) {
        if (node==null || depth>MAX_DEPTH || seen.contains(node)) return new Expr(null,null,"missing_cycle_or_depth");
        if (node.isConstant()) return new Expr(node,0L,"constant");
        PcodeOp def=node.getDef();
        if (def==null) return new Expr(node,0L,"leaf");
        Set<Varnode> next=new HashSet<Varnode>(seen); next.add(node);
        if (transparent(def) && def.getNumInputs()>=1) return trace(def.getInput(0),depth+1,next);
        int code=def.getOpcode();
        if (code==PcodeOp.LOAD) return new Expr(node,0L,"load");
        if (code==PcodeOp.MULTIEQUAL) return new Expr(node,null,"phi_ambiguous");
        if (def.getNumInputs()>=2 && (code==PcodeOp.PTRSUB || code==PcodeOp.PTRADD || code==PcodeOp.INT_ADD || code==PcodeOp.INT_SUB)) {
            Varnode a=def.getInput(0), b=def.getInput(1), base=a; Long delta=null;
            try {
                if (b!=null && b.isConstant()) {
                    delta=b.getOffset();
                    if (code==PcodeOp.INT_SUB) delta=Math.negateExact(delta);
                    if (code==PcodeOp.PTRADD) {
                        if (def.getNumInputs()<3 || !def.getInput(2).isConstant()) return new Expr(node,null,"ptradd_unknown_scale");
                        delta=Math.multiplyExact(delta,def.getInput(2).getOffset());
                    }
                } else if (code==PcodeOp.INT_ADD && a!=null && a.isConstant()) { delta=a.getOffset(); base=b; }
                if (delta!=null) {
                    Expr child=trace(base,depth+1,next);
                    if (child.offset==null) return child;
                    return new Expr(child.root,Math.addExact(child.offset,delta),"base_plus_constant");
                }
            } catch (ArithmeticException ignored) { return new Expr(node,null,"offset_overflow"); }
        }
        return new Expr(node,null,"opaque_"+def.getMnemonic());
    }

    private int parameterIndex(HighFunction high, Varnode node) {
        if (high==null || node==null || node.getHigh()==null) return -1;
        LocalSymbolMap map=high.getLocalSymbolMap(); if (map==null) return -1;
        HighVariable candidate=node.getHigh();
        for (int i=0;i<map.getNumParams();i++) {
            HighSymbol sym=map.getParamSymbol(i); if (sym==null || sym.getHighVariable()==null) continue;
            HighVariable param=sym.getHighVariable();
            if (candidate==param) return i;
            if (candidate.getRepresentative()!=null && param.getRepresentative()!=null && candidate.getRepresentative().equals(param.getRepresentative())) return i;
        }
        return -1;
    }

    private String hex(Long value) {
        if (value==null) return "-";
        return value<0 ? "-0x"+Long.toUnsignedString(-value,16) : "0x"+Long.toUnsignedString(value,16);
    }

    private List<String> readEntries(String path) throws Exception {
        List<String> entries=new ArrayList<String>(); Set<String> unique=new HashSet<String>();
        try (BufferedReader in=Files.newBufferedReader(Paths.get(path),StandardCharsets.UTF_8)) {
            String header=in.readLine();
            if (!"function_entry\telf_entry\tsymbol_names".equals(header)) throw new IllegalArgumentException("bad ControlsButton manifest header");
            String line; while ((line=in.readLine())!=null) {
                if (line.trim().isEmpty()) continue;
                String[] c=line.split("\t",-1);
                if (c.length!=3 || !c[0].matches("[0-9a-f]{8}") || !unique.add(c[0])) throw new IllegalArgumentException("bad/duplicate method row: "+line);
                entries.add(c[0]);
            }
        }
        if (entries.size()!=19) throw new IllegalArgumentException("expected 19 ControlsButton method entries");
        return entries;
    }

    private void header(BufferedWriter out) throws Exception {
        out.write("format\trow_type\tfunction_entry\tfunction_name\top_address\top_time\tdecompile_status\taddress_kind\taddress_root_token\taddress_offset\taddress_parameter_index\tvalue_token\tcall_kind\tcall_target_token\tparam0_arg_indices\tparam0_arg_offsets\tunknown_arg_indices\tstore_count\tcall_count\tunknown_store_count\tparam0_call_count\tunknown_call_arg_count");
        out.newLine();
    }

    private void row(BufferedWriter out,String type,Function fn,String entry,PcodeOp op,String status,String addressKind,String addressRoot,String addressOffset,String addressParam,String value,String callKind,String callTarget,String p0idx,String p0offs,String unknown,int stores,int calls,int unknownStores,int p0calls,int unknownArgs) throws Exception {
        SequenceNumber seq=op==null?null:op.getSeqnum();
        out.write(String.join("\t",FORMAT,type,entry,cell(fn==null?null:fn.getName(true)),cell(seq==null?null:seq.getTarget()),cell(seq==null?null:seq.getTime()),status,addressKind,addressRoot,addressOffset,addressParam,value,callKind,callTarget,p0idx,p0offs,unknown,Integer.toString(stores),Integer.toString(calls),Integer.toString(unknownStores),Integer.toString(p0calls),Integer.toString(unknownArgs)));
        out.newLine();
    }

    public void run() throws Exception {
        String[] args=getScriptArgs();
        if (args.length!=2) throw new IllegalArgumentException("Usage: ExportControlsButtonOwnerFieldAliasInventory.java <methods.tsv> <output.tsv>");
        List<String> entries=readEntries(args[0]); File output=new File(args[1]); if (output.getParentFile()!=null) output.getParentFile().mkdirs();
        DecompInterface decompiler=new DecompInterface(); if (!decompiler.openProgram(currentProgram)) throw new IllegalStateException("cannot open program");
        int completed=0;
        try (BufferedWriter out=new BufferedWriter(new OutputStreamWriter(new FileOutputStream(output),StandardCharsets.UTF_8))) {
            header(out);
            for (String entry:entries) {
                if (monitor.isCancelled()) throw new IllegalStateException("probe cancelled");
                Address a=toAddr(entry); Function fn=currentProgram.getFunctionManager().getFunctionAt(a);
                if (fn==null) throw new IllegalStateException("missing function "+entry);
                DecompileResults dr=decompiler.decompileFunction(fn,60,monitor);
                if (dr==null || !dr.decompileCompleted() || dr.getHighFunction()==null) {
                    row(out,"summary",fn,entry,null,"decompile_failed","-","-","-","-","-","-","-","-","-","-",0,0,0,0,0); continue;
                }
                HighFunction high=dr.getHighFunction(); int stores=0,calls=0,unknownStores=0,p0calls=0,unknownArgs=0;
                List<PcodeOpAST> ops=new ArrayList<PcodeOpAST>(); Iterator<PcodeOpAST> it=high.getPcodeOps(); while(it.hasNext()) ops.add(it.next());
                for (PcodeOpAST op:ops) {
                    int code=op.getOpcode();
                    if (code==PcodeOp.STORE && op.getNumInputs()>=3) {
                        Expr e=trace(op.getInput(1),0,new HashSet<Varnode>()); int pi=parameterIndex(high,e.root);
                        boolean unknown=e.offset==null || pi<0 || e.kind.startsWith("opaque_") || "phi_ambiguous".equals(e.kind); if (unknown) unknownStores++;
                        row(out,"store",fn,entry,op,"ok",e.kind,token(e.root),hex(e.offset),pi<0?"-":Integer.toString(pi),token(op.getInput(2)),"-","-","-","-","-",0,0,0,0,0); stores++;
                    } else if (code==PcodeOp.CALL || code==PcodeOp.CALLIND) {
                        List<String> indices=new ArrayList<String>(), offsets=new ArrayList<String>(), unresolved=new ArrayList<String>();
                        for (int i=1;i<op.getNumInputs();i++) {
                            Expr e=trace(op.getInput(i),0,new HashSet<Varnode>()); int pi=parameterIndex(high,e.root);
                            if (pi==0 && e.offset!=null) { indices.add(Integer.toString(i-1)); offsets.add(hex(e.offset)); }
                            else if (e.offset==null || (pi<0 && !"constant".equals(e.kind))) { unresolved.add(Integer.toString(i-1)); unknownArgs++; }
                        }
                        if (!indices.isEmpty()) p0calls++;
                        row(out,"call",fn,entry,op,"ok","-","-","-","-","-",code==PcodeOp.CALL?"CALL":"CALLIND",token(op.getInput(0)),indices.isEmpty()?"-":String.join(",",indices),offsets.isEmpty()?"-":String.join(",",offsets),unresolved.isEmpty()?"-":String.join(",",unresolved),0,0,0,0,0); calls++;
                    }
                }
                row(out,"summary",fn,entry,null,"ok","-","-","-","-","-","-","-","-","-","-",stores,calls,unknownStores,p0calls,unknownArgs); completed++;
            }
        } finally { decompiler.dispose(); }
        println("CONTROLSBUTTON_OWNER_FIELD_ALIAS_FORMAT="+FORMAT);
        println("CONTROLSBUTTON_OWNER_FIELD_ALIAS_METHODS="+entries.size());
        println("CONTROLSBUTTON_OWNER_FIELD_ALIAS_DECOMPILED="+completed);
        println("CONTROLSBUTTON_OWNER_FIELD_ALIAS_OUTPUT="+output.getAbsolutePath());
        println("CONTROLSBUTTON_OWNER_FIELD_ALIAS_AFFECTS_ROUTING=no");
    }
}
