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

// Evidence-only refinement of owner alias flow. Unlike the v22 inventory, this
// distinguishes the ADDRESS of this/this+field from the VALUE loaded out of a
// field. Passing LOAD(this+field) to a method is not an alias to the pointer
// slot itself and therefore cannot by itself replace that slot.
public class ExportClassOwnerAliasFlow extends GhidraScript {
    private static final String FORMAT = "NEVERGONE.GhidraClassOwnerAliasFlow/1";
    private static final int MAX_DEPTH = 28;

    private static class Expr {
        final Varnode root; final Long offset; final String kind;
        Expr(Varnode root, Long offset, String kind) { this.root=root; this.offset=offset; this.kind=kind; }
    }
    private static class Flow {
        final String kind; final Long offset;
        Flow(String kind, Long offset) { this.kind=kind; this.offset=offset; }
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
    private String parameterType(HighFunction high, int index) {
        if (high==null || index<0) return "-";
        LocalSymbolMap map=high.getLocalSymbolMap(); if (map==null || index>=map.getNumParams()) return "-";
        HighSymbol s=map.getParamSymbol(index); if (s==null || s.getHighVariable()==null) return "-";
        DataType t=s.getHighVariable().getDataType(); return t==null?"-":cell(t.getDisplayName());
    }
    private String parameterName(HighFunction high, int index) {
        if (high==null || index<0) return "-";
        LocalSymbolMap map=high.getLocalSymbolMap(); if (map==null || index>=map.getNumParams()) return "-";
        HighSymbol s=map.getParamSymbol(index); return s==null?"-":cell(s.getName());
    }
    private boolean ownerParameterProven(HighFunction high, String cls) {
        LocalSymbolMap map=high==null?null:high.getLocalSymbolMap();
        if (map==null || map.getNumParams()<1) return false;
        String type=parameterType(high,0).replace("class ","").replace("struct ","").replace(" ","");
        String want=(cls+"*").replace(" ","");
        if (type.equals(want) || type.endsWith("::"+want)) return true;
        String name=parameterName(high,0);
        if (!("this".equals(name) || "__this".equals(name))) return false;
        int split=cls.lastIndexOf("::");
        String shortCls=split<0?cls:cls.substring(split+2);
        String shortWant=(shortCls+"*").replace(" ","");
        return type.equals(shortWant) || type.endsWith("::"+shortWant) || type.contains(cls.replace(" ",""));
    }
    private Expr traceAddress(Varnode node, int depth, Set<Varnode> active) {
        if (node==null || depth>MAX_DEPTH || active.contains(node)) return new Expr(null,null,"missing_cycle_or_depth");
        if (node.isConstant()) return new Expr(node,0L,"constant");
        PcodeOp d=node.getDef(); if (d==null) return new Expr(node,0L,"leaf");
        Set<Varnode> next=new HashSet<Varnode>(active); next.add(node);
        if (transparent(d) && d.getNumInputs()>=1) return traceAddress(d.getInput(0),depth+1,next);
        int c=d.getOpcode();
        if (c==PcodeOp.LOAD) return new Expr(node,0L,"load_value");
        if (c==PcodeOp.MULTIEQUAL) {
            Expr common=null;
            for (int i=0;i<d.getNumInputs();i++) {
                Expr e=traceAddress(d.getInput(i),depth+1,next);
                if (e.root==null || e.offset==null) return new Expr(node,null,"phi_ambiguous");
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
                    Expr e=traceAddress(base,depth+1,next); if (e.offset==null) return new Expr(node,null,"unresolved_arithmetic_base");
                    return new Expr(e.root,Math.addExact(e.offset,delta),"base_plus_constant");
                }
            } catch (ArithmeticException ignored) { return new Expr(node,null,"offset_overflow"); }
        }
        return new Expr(node,null,"opaque_"+d.getMnemonic());
    }
    private boolean containsParam(HighFunction high,Varnode node,int want,int depth,Set<Varnode> seen) {
        if (node==null || depth>MAX_DEPTH || !seen.add(node)) return false;
        if (parameterIndex(high,node)==want) return true;
        PcodeOp d=node.getDef(); if (d==null) return false;
        for (int i=0;i<d.getNumInputs();i++) if (containsParam(high,d.getInput(i),want,depth+1,seen)) return true;
        return false;
    }
    private Long exactLoadedOwnerField(HighFunction high,Varnode node,int depth,Set<Varnode> seen) {
        if (node==null || depth>MAX_DEPTH || !seen.add(node)) return null;
        PcodeOp d=node.getDef();
        if (d==null) return null;
        if (transparent(d) && d.getNumInputs()>=1) return exactLoadedOwnerField(high,d.getInput(0),depth+1,seen);
        if (d.getOpcode()==PcodeOp.LOAD && d.getNumInputs()>=2) {
            Expr a=traceAddress(d.getInput(1),0,new HashSet<Varnode>());
            if (a.root!=null && a.offset!=null && parameterIndex(high,a.root)==0) return a.offset;
            return null;
        }
        int c=d.getOpcode();
        if (c==PcodeOp.PTRSUB || c==PcodeOp.PTRADD || c==PcodeOp.INT_ADD || c==PcodeOp.INT_SUB || c==PcodeOp.MULTIEQUAL) {
            Long common=null;
            for (int i=0;i<d.getNumInputs();i++) {
                Varnode in=d.getInput(i); if (in==null || in.isConstant()) continue;
                Long x=exactLoadedOwnerField(high,in,depth+1,new HashSet<Varnode>(seen));
                if (x==null) continue;
                if (common==null) common=x; else if (!common.equals(x)) return null;
            }
            return common;
        }
        return null;
    }
    private Flow classifyFlow(HighFunction high,Varnode node,boolean ownerProven) {
        if (!ownerProven) return new Flow("owner_parameter_unproven",null);
        Expr e=traceAddress(node,0,new HashSet<Varnode>());
        if (e.root!=null && e.offset!=null && parameterIndex(high,e.root)==0) return new Flow("owner_address",e.offset);
        Long loaded=exactLoadedOwnerField(high,node,0,new HashSet<Varnode>());
        if (loaded!=null) return new Flow("owner_field_value",loaded);
        if (containsParam(high,node,0,0,new HashSet<Varnode>())) return new Flow("owner_derived_unknown",null);
        return new Flow("unrelated",null);
    }
    private Function directTarget(PcodeOp op) {
        if (op==null || op.getOpcode()!=PcodeOp.CALL || op.getNumInputs()<1) return null;
        Varnode t=op.getInput(0); Address a=null;
        try { if (t.isAddress()) a=t.getAddress(); else if (t.isConstant()) a=toAddr(Long.toHexString(t.getOffset())); } catch(Exception ignored) {}
        if (a==null) return null;
        Function f=currentProgram.getFunctionManager().getFunctionAt(a);
        if (f==null && (a.getOffset()&1L)!=0L) try { f=currentProgram.getFunctionManager().getFunctionAt(a.getAddressSpace().getAddress(a.getOffset()&~1L)); } catch(Exception ignored) {}
        if (f==null) f=currentProgram.getFunctionManager().getFunctionContaining(a);
        return f;
    }
    private boolean ownedBy(String cls,Function f) {
        String p=cls+"::"; String full=f.getName(true),shortName=f.getName();
        return (full!=null && full.startsWith(p)) || (shortName!=null && shortName.startsWith(p));
    }
    private String hex(Long v) { if(v==null)return "-"; return v<0?"-0x"+Long.toUnsignedString(-v,16):"0x"+Long.toUnsignedString(v,16); }
    private void header(BufferedWriter out) throws Exception {
        out.write("format\trow_type\tclass_name\tfunction_entry\tfunction_name\towner_parameter_proven\towner_parameter_type\towner_parameter_name\top_address\top_time\tcall_kind\tcall_target_entry\tcall_target_name\targ_index\targ_token\targ_flow_kind\targ_owner_offset\taddress_token\taddress_flow_kind\taddress_owner_offset\tvalue_token\tstore_count\tcall_count\towner_address_arg_count\towner_field_value_arg_count\towner_derived_unknown_arg_count\towner_derived_unknown_store_count"); out.newLine();
    }
    private void row(BufferedWriter out,String rowType,String cls,Function fn,HighFunction high,boolean owner,PcodeOp op,String callKind,String targetEntry,String targetName,String argIndex,String argToken,String argFlow,String argOffset,String addrToken,String addrFlow,String addrOffset,String value,int stores,int calls,int oa,int fv,int ua,int us) throws Exception {
        SequenceNumber s=op==null?null:op.getSeqnum();
        out.write(String.join("\t",FORMAT,rowType,cls,cell(fn==null?null:fn.getEntryPoint()),cell(fn==null?null:fn.getName(true)),owner?"1":"0",parameterType(high,0),parameterName(high,0),cell(s==null?null:s.getTarget()),cell(s==null?null:s.getTime()),callKind,targetEntry,targetName,argIndex,argToken,argFlow,argOffset,addrToken,addrFlow,addrOffset,value,Integer.toString(stores),Integer.toString(calls),Integer.toString(oa),Integer.toString(fv),Integer.toString(ua),Integer.toString(us))); out.newLine();
    }
    public void run() throws Exception {
        String[] args=getScriptArgs(); if(args.length!=2) throw new IllegalArgumentException("Usage: ExportClassOwnerAliasFlow.java <exact-class-name> <output.tsv>");
        String cls=args[0]; File output=new File(args[1]); if(output.getParentFile()!=null) output.getParentFile().mkdirs();
        List<Function> functions=new ArrayList<Function>(); FunctionIterator fi=currentProgram.getFunctionManager().getFunctions(true);
        while(fi.hasNext() && !monitor.isCancelled()) { Function f=fi.next(); if(ownedBy(cls,f)) functions.add(f); }
        if(functions.isEmpty()) throw new IllegalArgumentException("No functions found for exact class: "+cls);
        DecompInterface dec=new DecompInterface(); if(!dec.openProgram(currentProgram)) throw new IllegalStateException("Could not open program");
        try(BufferedWriter out=new BufferedWriter(new OutputStreamWriter(new FileOutputStream(output),StandardCharsets.UTF_8))) {
            header(out);
            for(Function fn:functions) {
                DecompileResults dr=dec.decompileFunction(fn,60,monitor);
                if(dr==null || !dr.decompileCompleted() || dr.getHighFunction()==null) {
                    row(out,"summary",cls,fn,null,false,null,
                        "-","-","-","-","-","-","-","-","-","-","-",
                        0,0,0,0,0,0);
                    continue;
                }
                HighFunction high=dr.getHighFunction(); boolean owner=ownerParameterProven(high,cls);
                int stores=0,calls=0,oa=0,fv=0,ua=0,us=0;
                Iterator<PcodeOpAST> it=high.getPcodeOps();
                while(it.hasNext()) {
                    PcodeOpAST op=it.next(); int c=op.getOpcode();
                    if(c==PcodeOp.STORE && op.getNumInputs()>=3) {
                        stores++; Flow f=classifyFlow(high,op.getInput(1),owner); if("owner_derived_unknown".equals(f.kind)) us++;
                        row(out,"store",cls,fn,high,owner,op,"-","-","-","-","-","-","-",token(op.getInput(1)),f.kind,hex(f.offset),token(op.getInput(2)),0,0,0,0,0,0);
                    } else if(c==PcodeOp.CALL || c==PcodeOp.CALLIND) {
                        calls++; Function target=directTarget(op); String ck=c==PcodeOp.CALL?"CALL":"CALLIND";
                        if(op.getNumInputs()<=1) row(out,"call",cls,fn,high,owner,op,ck,cell(target==null?null:target.getEntryPoint()),cell(target==null?null:target.getName(true)),"-","-","-","-","-","-","-","-",0,0,0,0,0,0);
                        for(int i=1;i<op.getNumInputs();i++) {
                            Flow f=classifyFlow(high,op.getInput(i),owner);
                            if("owner_address".equals(f.kind)) oa++; else if("owner_field_value".equals(f.kind)) fv++; else if("owner_derived_unknown".equals(f.kind)) ua++;
                            row(out,"call_arg",cls,fn,high,owner,op,ck,cell(target==null?null:target.getEntryPoint()),cell(target==null?null:target.getName(true)),Integer.toString(i-1),token(op.getInput(i)),f.kind,hex(f.offset),"-","-","-","-",0,0,0,0,0,0);
                        }
                    }
                }
                row(out,"summary",cls,fn,high,owner,null,
                    "-","-","-","-","-","-","-","-","-","-","-",
                    stores,calls,oa,fv,ua,us);
            }
        } finally { dec.dispose(); }
        println("CLASS_OWNER_ALIAS_FLOW_FORMAT="+FORMAT); println("CLASS_OWNER_ALIAS_FLOW_CLASS="+cls); println("CLASS_OWNER_ALIAS_FLOW_FUNCTIONS="+functions.size()); println("CLASS_OWNER_ALIAS_FLOW_OUTPUT="+output.getAbsolutePath()); println("CLASS_OWNER_ALIAS_FLOW_AFFECTS_ROUTING=no");
    }
}
