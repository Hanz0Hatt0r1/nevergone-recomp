package nevergone;

import com.github.unidbg.AndroidEmulator;
import com.github.unidbg.Emulator;
import com.github.unidbg.LibraryResolver;
import com.github.unidbg.Module;
import com.github.unidbg.debugger.McpToolkit;
import com.github.unidbg.debugger.McpTool;
import com.github.unidbg.arm.backend.Unicorn2Factory;
import com.github.unidbg.linux.android.AndroidEmulatorBuilder;
import com.github.unidbg.linux.android.AndroidResolver;
import com.github.unidbg.linux.android.ElfLibraryFile;
import com.github.unidbg.linux.android.dvm.DalvikModule;
import com.github.unidbg.linux.android.dvm.VM;
import com.github.unidbg.linux.LinuxModule;
import com.github.unidbg.linux.ModuleSymbol;
import com.github.unidbg.spi.LibraryFile;

import java.io.File;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.HashMap;
import java.util.Map;

/** Minimal 32-bit Nevergone loader. Native calls need appropriate arguments and environment. */
public final class NevergoneHarness {
    private static final File LIB_DIR = new File(System.getenv("NEVERGONE_LIB_DIR"));
    private static final File DEVICE_LIB_DIR = new File(System.getenv("NEVERGONE_DEVICE_LIB_DIR"));
    private static final File DEVICE_RUNTIME_DIR = new File(System.getenv("NEVERGONE_DEVICE_RUNTIME_DIR"));

    public static void main(String[] args) throws Exception {
        boolean smoke = false;
        boolean jniOnLoad = true;
        boolean deviceRuntime = true;
        boolean probeEgl = false;
        boolean probe = false;
        boolean probeEnemyActionsCtor = false;
        String probePlan = null;
        for (int i = 0; i < args.length; i++) {
            String arg = args[i];
            if ("--smoke".equals(arg)) smoke = true;
            else if ("--no-jni-onload".equals(arg)) jniOnLoad = false;
            else if ("--device-runtime".equals(arg)) deviceRuntime = true;
            else if ("--sdk23-runtime".equals(arg)) deviceRuntime = false;
            else if ("--probe-egl".equals(arg)) probeEgl = true;
            else if ("--probe".equals(arg)) probe = true;
            else if ("--probe-enemy-actions-ctor".equals(arg)) probeEnemyActionsCtor = true;
            else if ("--probe-plan".equals(arg) && i + 1 < args.length) probePlan = args[++i];
            else throw new IllegalArgumentException("Unknown option: " + arg);
        }
        final boolean useDeviceRuntime = deviceRuntime;
        for (String name : new String[] {"libffmpeg.so", "libcutils.so", "libEGL.so",
                "libGLESv2.so", "libcocos2dcpp.so"}) {
            if (!new File(LIB_DIR, name).isFile()) {
                throw new IllegalStateException("Missing library: " + new File(LIB_DIR, name));
            }
        }
        AndroidEmulator emulator = AndroidEmulatorBuilder.for32Bit()
                .setProcessName("com.hippiegame.nevergone")
                .addBackendFactory(new Unicorn2Factory(true))
                .build();
        try {
            // Android platform constructors can require more OS state than the loader provides.
            emulator.getMemory().setCallInitFunction(false);
            final Map<String, File> selectedFiles = new HashMap<>();
            // The root ElfLibraryFile resolves siblings before invoking the resolver.
            for (File source : LIB_DIR.listFiles()) {
                if (source.isFile() && source.getName().endsWith(".so"))
                    selectedFiles.put(source.getName(), source);
            }
            AndroidResolver android = new AndroidResolver(23, "liblog.so", "libz.so",
                    "libdl.so", "libstdc++.so", "libm.so", "libc.so");
            emulator.getMemory().setLibraryResolver(new LibraryResolver() {
                @Override public void onSetToLoader(Emulator<?> emu) {
                    android.onSetToLoader(emu);
                }
                @Override public LibraryFile resolveLibrary(Emulator<?> emu, String name) {
                    String baseName = new File(name).getName();
                    LibraryFile selected = null;
                    if (baseName.endsWith(".so")) {
                        File[] dirs = useDeviceRuntime
                                ? new File[] {LIB_DIR, DEVICE_LIB_DIR, DEVICE_RUNTIME_DIR}
                                : new File[] {LIB_DIR, DEVICE_LIB_DIR};
                        for (File dir : dirs) {
                            File local = new File(dir, baseName);
                            if (local.isFile()) { selected = new ElfLibraryFile(local, false); selectedFiles.put(baseName, local); break; }
                        }
                    }
                    if (selected == null) selected = android.resolveLibrary(emu, name);
                    if (selected == null) return null;
                    final LibraryFile delegate = selected;
                    final LibraryResolver resolver = this;
                    return new LibraryFile() {
                        public long getFileSize() { return delegate.getFileSize(); }
                        public String getName() { return delegate.getName(); }
                        public String getMapRegionName() { return delegate.getMapRegionName(); }
                        public String getPath() { return delegate.getPath(); }
                        public ByteBuffer mapBuffer() throws IOException { return delegate.mapBuffer(); }
                        public LibraryFile resolveLibrary(Emulator<?> e, String dependency) {
                            return resolver.resolveLibrary(e, dependency);
                        }
                    };
                }
            });
            VM vm = emulator.createDalvikVM();
            DalvikModule dm = vm.loadLibrary(new File(LIB_DIR, "libcocos2dcpp.so"), false);
            Module module = dm.getModule();
            Relr32.apply(emulator, selectedFiles);
            if (jniOnLoad) {
                dm.callJNI_OnLoad(emulator);
                System.out.println("JNI_OnLoad completed");
            }
            System.out.printf("LOADED libcocos2dcpp.so base=0x%x size=0x%x%n", module.base, module.size);
            System.out.printf("JNI_OnLoad=%s%n", module.findSymbolByName("JNI_OnLoad", false));
            System.out.printf("FFMPEG=%s%n", emulator.getMemory().findModule("libffmpeg.so"));
            Module gles = emulator.getMemory().findModule("libGLESv2.so");
            if (gles == null) throw new IllegalStateException("libGLESv2.so did not load");
            System.out.printf("GLESv2=%s%n", gles);
            for (String name : new String[] {"libEGL.so", "libcutils.so"}) {
                Module dependency = emulator.getMemory().findModule(name);
                if (dependency == null) throw new IllegalStateException(name + " did not load");
                System.out.printf("%s=%s%n", name, dependency);
            }
            for (String name : new String[] {"libcocos2dcpp.so", "libGLESv2.so",
                    "libEGL.so", "libcutils.so"}) {
                LinuxModule loaded = (LinuxModule) emulator.getMemory().findModule(name);
                System.out.printf("UNRESOLVED %s=%d%n", name, loaded.getUnresolvedSymbol().size());
                if ("libEGL.so".equals(name) || "libcutils.so".equals(name)) {
                    for (ModuleSymbol unresolved : loaded.getUnresolvedSymbol()) {
                        System.out.printf("  %s%n", unresolved.getSymbol().getName());
                    }
                }
            }
            int totalUnresolved = 0;
            int moduleCount = 0;
            for (Module loaded : emulator.getMemory().getLoadedModules()) {
                moduleCount++;
                System.out.printf("MODULE %s base=0x%x size=0x%x%n", loaded.name, loaded.base, loaded.size);
                LinuxModule linux = (LinuxModule) loaded;
                totalUnresolved += linux.getUnresolvedSymbol().size();
                if (!linux.getUnresolvedSymbol().isEmpty() && !"libEGL.so".equals(loaded.name)
                        && !"libcutils.so".equals(loaded.name)) {
                    System.out.printf("UNRESOLVED %s=%d%n", loaded.name,
                            linux.getUnresolvedSymbol().size());
                    for (ModuleSymbol unresolved : linux.getUnresolvedSymbol()) {
                        System.out.printf("  %s%n", unresolved.getSymbol().getName());
                    }
                }
            }
            System.out.printf("MODULES=%d UNRESOLVED_TOTAL=%d%n", moduleCount, totalUnresolved);
            if (probeEgl) {
                Number result = emulator.getMemory().findModule("libEGL.so").callFunction(emulator, "eglGetError");
                System.out.printf("EGL_GET_ERROR=0x%x%n", result.intValue());
                if (result.intValue() != 0x3000)
                    throw new IllegalStateException("Fresh EGL probe did not return EGL_SUCCESS; inspect emulator faults");
                return;
            }
            if (probe) { EvidenceProbe.run(emulator, module); return; }
            if (probePlan != null) { ProbeRunner.run(emulator, module, new File(probePlan)); return; }
            if (probeEnemyActionsCtor) { EnemyActionsConstructorProbe.run(emulator, module); return; }
            if (smoke) return;

            // Constructors are deferred. Some need app state; JNI_OnLoad is tested.
            McpToolkit toolkit = new McpToolkit();
            toolkit.addTool(new McpTool() {
                @Override public String name() { return "module_info"; }
                @Override public String description() { return "Print Nevergone module base and JNI_OnLoad address"; }
                @Override public String[] paramNames() { return new String[0]; }
                @Override public void execute(String[] params) {
                    System.out.printf("libcocos2dcpp base=0x%x JNI_OnLoad=%s%n",
                            module.base, module.findSymbolByName("JNI_OnLoad", false));
                }
            });
            System.out.println("Debugger ready. At prompt enter: mcp 9239");
            toolkit.run(emulator.attach());
        } finally {
            emulator.close();
        }
    }
}
