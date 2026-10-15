import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
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

// Evidence-only alias inventory for every method owned by one exact C++ class.
// It records all STORE/CALL/CALLIND operations, exact parameter-0 affine offsets,
// unresolved parameter-0-derived expressions, and direct call targets. It never
// infers a receiver class and never modifies the program.
public class ExportClassOwnerAliasInventory extends GhidraScript {
    private static final String FORMAT = "NEVERGONE.GhidraClassOwnerAliasInventory/1";
    private static final int MAX_DEPTH = 24;

    private static class Expr {
        final Varnode root; final Long offset; final String kind;
        Expr(Varnode root, Long offset, String kind) { this.root=root; this.offset=offset; this.kind=kind; }
    }

    private String cell(Object v) {
        if (v==null) return "-";
        return String.valueOf(v).replace('\t',' ').replace('\n',' ').replace('\r',' ');
    }
    private String token(Varnode n) {
        if (n==null) return "-";
        if (n.isConstant()) return "const:0x"+Long.toUnsignedString(n.getOffset(),16)+":"+n.getSize();
        PcodeOp d=n.getDef();
        if (d!=null) { SequenceNumber s=d.getSeqnum(); return "def:"+d.getMnemonic()+"@"+cell(s.getTarget())+":"+s.getTime()+":"+n.getSize(); }
        String k=n.isRegister()?"register":n.isUnique()?"unique":n.isAddress()?"address":"input";
        return k+":"+cell(n.getAddress())+":"+n.getSize();
    }
    private boolean transparent(PcodeOp op) {
        if (op==null) return false; int c=op.getOpcode();
        return c==PcodeOp.COPY || c==PcodeOp.CAST || c==PcodeOp.INDIRECT;
    }
    private int parameterIndex(HighFunction high, Varnode node) {
        if (high==null || node==null || node.getHigh()==null) return -1;
        LocalSymbolMap map=high.getLocalSymbolMap(); if (map==null) return -1;
        HighVariable h=node.getHigh();
        for (int i=0;i<map.getNumParams();i++) {
            HighSymbol s=map.getParamSymbol(i); if (s==null || s.getHighVariable()==null) continue;
            HighVariable p=s.getHighVariable();
            if (h==p) return i;
            if (h.getRepresentative()!=null && p.getRepresentative()!=null && h.getRepresentative().equals(p.getRepresentative())) return i;
        }
        return -1;
    }
    private Expr trace(Varnode node, int depth, Set<Varnode> active) {
        if (node==null || depth>MAX_DEPTH || active.contains(node)) return new Expr(null,null,"missing_cycle_or_depth");
        if (node.isConstant()) return new Expr(node,0L,"constant");
        PcodeOp d=node.getDef(); if (d==null) return new Expr(node,0L,"leaf");
        Set<Varnode> next=new HashSet<Varnode>(active); next.add(node);
        if (transparent(d) && d.getNumInputs()>=1) return trace(d.getInput(0),depth+1,next);
        int c=d.getOpcode();
        if (c==PcodeOp.LOAD) return new Expr(node,0L,"load");
        if (c==PcodeOp.MULTIEQUAL) {
            Expr common=null;
            for (int i=0;i<d.getNumInputs();i++) {
                Expr e=trace(d.getInput(i),depth+1,next);
                if (e.offset==null || e.root==null) return new Expr(node,null,"phi_ambiguous");
                if (common==null) common=e;
                else if (!common.root.equals(e.root) || !common.offset.equals(e.offset)) return new Expr(node,null,"phi_ambiguous");
            }
            return common==null?new Expr(node,null,"phi_empty"):common;
        }
        if (d.getNumInputs()>=2 && (c==PcodeOp.PTRSUB || c==PcodeOp.PTRADD || c==PcodeOp.INT_ADD || c==PcodeOp.INT_SUB)) {
            Varnode a=d.getInput(0), b=d.getInput(1), base=a; Long delta=null;
            try {
                if (b!=null && b.isConstant()) {
                    delta=b.getOffset(); if (c==PcodeOp.INT_SUB) delta=Math.negateExact(delta);
                    if (c==PcodeOp.PTRADD) {
                        if (d.getNumInputs()<3 || !d.getInput(2).isConstant()) return new Expr(node,null,"ptradd_unknown_scale");
                        delta=Math.multiplyExact(delta,d.getInput(2).getOffset());
                    }
                } else if (c==PcodeOp.INT_ADD && a!=null && a.isConstant()) { delta=a.getOffset(); base=b; }
                if (delta!=null) {
                    Expr e=trace(base,depth+1,next); if (e.offset==null) return new Expr(node,null,"unresolved_arithmetic_base");
                    return new Expr(e.root,Math.addExact(e.offset,delta),"base_plus_constant");
                }
            } catch (ArithmeticException ignored) { return new Expr(node,null,"offset_overflow"); }
        }
        return new Expr(node,null,"opaque_"+d.getMnemonic());
    }
    private boolean containsParam(HighFunction high, Varnode node, int want, int depth, Set<Varnode> seen) {
        if (node==null || depth>MAX_DEPTH || !seen.add(node)) return false;
        if (parameterIndex(high,node)==want) return true;
        PcodeOp d=node.getDef(); if (d==null) return false;
        for (int i=0;i<d.getNumInputs();i++) if (containsParam(high,d.getInput(i),want,depth+1,seen)) return true;
        return false;
    }
    private Function directTarget(PcodeOp op) {
        if (op==null || op.getOpcode()!=PcodeOp.CALL || op.getNumInputs()<1) return null;
        Varnode t=op.getInput(0); Address a=null;
        try {
            if (t.isAddress()) a=t.getAddress(); else if (t.isConstant()) a=toAddr(Long.toHexString(t.getOffset()));
        } catch (Exception ignored) {}
        if (a==null) return null;
        Function f=currentProgram.getFunctionManager().getFunctionAt(a);
        if (f==null && (a.getOffset()&1L)!=0L) try { f=currentProgram.getFunctionManager().getFunctionAt(a.getAddressSpace().getAddress(a.getOffset()&~1L)); } catch(Exception ignored) {}
        if (f==null) f=currentProgram.getFunctionManager().getFunctionContaining(a);
        return f;
    }
    private boolean ownedBy(String cls, Function f) {
        String p=cls+"::"; String full=f.getName(true), shortName=f.getName();
        return (full!=null && full.startsWith(p)) || (shortName!=null && shortName.startsWith(p));
    }
    private String hex(Long v) { if (v==null) return "-"; return v<0?"-0x"+Long.toUnsignedString(-v,16):"0x"+Long.toUnsignedString(v,16); }
    private String join(List<String> xs) { return xs.isEmpty()?"-":String.join(",",xs); }
    private void header(BufferedWriter out) throws Exception {
        out.write("format\trow_type\tclass_name\tfunction_entry\tfunction_name\top_address\top_time\tdecompile_status\taddress_kind\taddress_root_token\taddress_offset\taddress_parameter_index\taddress_contains_param0\tvalue_token\tcall_kind\tcall_target_entry\tcall_target_name\tparam0_arg_indices\tparam0_arg_offsets\tparam0_unknown_arg_indices\tstore_count\tcall_count\tparam0_unknown_store_count\tparam0_call_count\tparam0_unknown_call_arg_count"); out.newLine();
    }
    private void write(BufferedWriter out,String type,String cls,Function fn,PcodeOp op,String status,String ak,String ar,String ao,String ap,String ac,String value,String ck,String cte,String ctn,String pidx,String poff,String punk,int stores,int calls,int us,int pc,int ua) throws Exception {
        SequenceNumber s=op==null?null:op.getSeqnum();
        out.write(String.join("\t",FORMAT,type,cls,cell(fn==null?null:fn.getEntryPoint()),cell(fn==null?null:fn.getName(true)),cell(s==null?null:s.getTarget()),cell(s==null?null:s.getTime()),status,ak,ar,ao,ap,ac,value,ck,cte,ctn,pidx,poff,punk,Integer.toString(stores),Integer.toString(calls),Integer.toString(us),Integer.toString(pc),Integer.toString(ua))); out.newLine();
    }
    public void run() throws Exception {
        String[] args=getScriptArgs(); if (args.length!=2) throw new IllegalArgumentException("Usage: ExportClassOwnerAliasInventory.java <exact-class-name> <output.tsv>");
        String cls=args[0]; File output=new File(args[1]); if (output.getParentFile()!=null) output.getParentFile().mkdirs();
        List<Function> functions=new ArrayList<Function>(); FunctionIterator itf=currentProgram.getFunctionManager().getFunctions(true);
        while(itf.hasNext() && !monitor.isCancelled()) { Function f=itf.next(); if(ownedBy(cls,f)) functions.add(f); }
        if(functions.isEmpty()) throw new IllegalArgumentException("No functions found for exact class: "+cls);
        DecompInterface dec=new DecompInterface(); if(!dec.openProgram(currentProgram)) throw new IllegalStateException("Could not open program");
        int completed=0,failed=0;
        try(BufferedWriter out=new BufferedWriter(new OutputStreamWriter(new FileOutputStream(output),StandardCharsets.UTF_8))) {
            header(out);
            for(Function fn:functions) {
                DecompileResults dr=dec.decompileFunction(fn,60,monitor);
                if(dr==null || !dr.decompileCompleted() || dr.getHighFunction()==null) { failed++; write(out,"summary",cls,fn,null,"decompile_failed","-","-","-","-","-","-","-","-","-","-","-","-",0,0,0,0,0); continue; }
                completed++; HighFunction high=dr.getHighFunction(); int stores=0,calls=0,unknownStores=0,p0calls=0,unknownArgs=0;
                Iterator<PcodeOpAST> ops=high.getPcodeOps();
                while(ops.hasNext()) {
                    PcodeOpAST op=ops.next(); int c=op.getOpcode();
                    if(c==PcodeOp.STORE && op.getNumInputs()>=3) {
                        stores++; Expr e=trace(op.getInput(1),0,new HashSet<Varnode>()); int pi=parameterIndex(high,e.root); boolean contains=containsParam(high,op.getInput(1),0,0,new HashSet<Varnode>());
                        if(contains && !(pi==0 && e.offset!=null)) unknownStores++;
                        write(out,"store",cls,fn,op,"ok",e.kind,token(e.root),hex(e.offset),pi<0?"-":Integer.toString(pi),contains?"1":"0",token(op.getInput(2)),"-","-","-","-","-","-",0,0,0,0,0);
                    } else if(c==PcodeOp.CALL || c==PcodeOp.CALLIND) {
                        calls++; List<String> idx=new ArrayList<String>(), offs=new ArrayList<String>(), unk=new ArrayList<String>();
                        for(int i=1;i<op.getNumInputs();i++) {
                            Expr e=trace(op.getInput(i),0,new HashSet<Varnode>()); int pi=parameterIndex(high,e.root); boolean contains=containsParam(high,op.getInput(i),0,0,new HashSet<Varnode>());
                            if(pi==0 && e.offset!=null) { idx.add(Integer.toString(i-1)); offs.add(hex(e.offset)); }
                            else if(contains) { unk.add(Integer.toString(i-1)); unknownArgs++; }
                        }
                        if(!idx.isEmpty() || !unk.isEmpty()) p0calls++;
                        Function target=directTarget(op);
                        write(out,"call",cls,fn,op,"ok","-","-","-","-","-","-",c==PcodeOp.CALL?"CALL":"CALLIND",cell(target==null?null:target.getEntryPoint()),cell(target==null?null:target.getName(true)),join(idx),join(offs),join(unk),0,0,0,0,0);
                    }
                }
                write(out,"summary",cls,fn,null,"ok","-","-","-","-","-","-","-","-","-","-","-","-",stores,calls,unknownStores,p0calls,unknownArgs);
            }
        } finally { dec.dispose(); }
        println("CLASS_OWNER_ALIAS_FORMAT="+FORMAT); println("CLASS_OWNER_ALIAS_CLASS="+cls); println("CLASS_OWNER_ALIAS_FUNCTIONS="+functions.size()); println("CLASS_OWNER_ALIAS_DECOMPILED="+completed); println("CLASS_OWNER_ALIAS_FAILED="+failed); println("CLASS_OWNER_ALIAS_OUTPUT="+output.getAbsolutePath()); println("CLASS_OWNER_ALIAS_AFFECTS_ROUTING=no");
    }
}
