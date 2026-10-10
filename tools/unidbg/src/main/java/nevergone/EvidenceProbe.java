package nevergone;
import com.alibaba.fastjson.JSONObject;
import com.github.unidbg.AndroidEmulator;
import com.github.unidbg.Module;
import com.github.unidbg.memory.MemoryBlock;
import com.github.unidbg.pointer.UnidbgPointer;

final class EvidenceProbe {
    static String hex(byte[] bytes) {
        StringBuilder result = new StringBuilder();
        for (byte b : bytes) result.append(String.format("%02x", b & 255));
        return result.toString();
    }
    static void emit(JSONObject row) { System.out.println("EVIDENCE " + row.toJSONString()); }
    static void run(AndroidEmulator emulator, Module module) {
        String decode = "_ZN7cocos2d6DecodeEPhiS0_j";
        int[] sizes = {0,1,2,126,127,128,253,254,255,1024};
        for (int key : new int[] {0,1,127,255,0x123}) for (int size : sizes)
            for (boolean inplace : new boolean[] {false,true}) {
                MemoryBlock block = emulator.getMemory().malloc(8192, true);
                try {
                    UnidbgPointer src = block.getPointer();
                    UnidbgPointer dst = src.share(inplace ? 0 : 4096, 0);
                    byte[] input = new byte[size];
                    for (int i = 0; i < size; i++) input[i] = (byte)(i * 73 + 19);
                    src.write(0, input, 0, size); dst.setByte(size, (byte)0x5a);
                    module.callFunction(emulator, decode, src, size, dst, key);
                    JSONObject row = new JSONObject(true);
                    row.put("kind", "asset_decode"); row.put("symbol", decode);
                    row.put("offset", "0x534a41"); row.put("size", size); row.put("key", key);
                    row.put("inplace", inplace); row.put("input_pattern", "(i*73+19)&255");
                    row.put("output_hex", hex(dst.getByteArray(0,size)));
                    row.put("guard", dst.getByte(size) & 255); emit(row);
                } finally { block.free(); }
            }
        String contains = "_ZNK7cocos2d6CCRect13containsPointERKNS_7CCPointE";
        String intersects = "_ZNK7cocos2d6CCRect14intersectsRectERKS0_";
        MemoryBlock block = emulator.getMemory().malloc(4096,true);
        try {
            UnidbgPointer rect = block.getPointer(), other = rect.share(64,0);
            float[] fixed = {10,20,30,40};
            for (int i=0;i<4;i++) rect.setFloat(i*4,fixed[i]);
            for (float[] point : new float[][] {{10,20},{40,60},{25,40},{9.999f,20},{40.001f,60},{Float.NaN,30}}) {
                other.setFloat(0,point[0]); other.setFloat(4,point[1]);
                int answer = module.callFunction(emulator,contains,rect,other).intValue();
                JSONObject row = new JSONObject(true); row.put("kind","contains_point");
                row.put("symbol",contains); row.put("offset","0x517a3b"); row.put("rect",fixed);
                row.put("point",new String[] {Float.toString(point[0]),Float.toString(point[1])});
                row.put("result",answer); emit(row);
            }
            for (float[] input : new float[][] {{40,20,1,1},{40.001f,20,1,1},{10,60,1,1},{10,60.001f,1,1},{25,30,0,0},{41,61,1,1}}) {
                for (int i=0;i<4;i++) other.setFloat(i*4,input[i]);
                int answer=module.callFunction(emulator,intersects,rect,other).intValue();
                JSONObject row = new JSONObject(true); row.put("kind","intersects_rect");
                row.put("symbol",intersects); row.put("offset","0x517aaf"); row.put("rect",fixed);
                row.put("other",input); row.put("result",answer); emit(row);
            }
        } finally { block.free(); }
    }
}
