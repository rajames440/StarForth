/*  Title:      Tools/Setup/src/Build.java
    Author:     Makarius

Build Isabelle/Scala/Java modules.
*/

package isabelle.setup;


import java.io.BufferedOutputStream;
import java.io.CharArrayWriter;
import java.io.File;
import java.io.IOException;
import java.io.PrintStream;
import java.math.BigInteger;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.LinkedList;
import java.util.List;
import java.util.Locale;
import java.util.Properties;
import java.util.TreeMap;
import java.util.jar.Attributes;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;
import java.util.jar.JarOutputStream;
import java.util.jar.Manifest;
import java.util.stream.Stream;

import javax.tools.JavaCompiler;
import javax.tools.JavaFileObject;
import javax.tools.StandardJavaFileManager;
import javax.tools.ToolProvider;

import dotty.tools.dotc.Driver;
import dotty.tools.dotc.interfaces.Diagnostic;
import dotty.tools.dotc.interfaces.ReporterResult;
import dotty.tools.dotc.interfaces.SimpleReporter;


public class Build
{
    /** SHA1 digest **/

    @FunctionalInterface
    public interface Digest_Body
    {
      void apply(MessageDigest sha) throws IOException, InterruptedException;
    }

    public static String make_digest(Digest_Body body)
      throws NoSuchAlgorithmException, IOException, InterruptedException
    {
        MessageDigest sha = MessageDigest.getInstance("SHA");
        body.apply(sha);
        return String.format(Locale.ROOT, "%040x", new BigInteger(1, sha.digest()));
    }

    public static String make_shasum(String name, Digest_Body body)
      throws NoSuchAlgorithmException, IOException, InterruptedException
    {
        return make_digest(body) + " " + name + "\n";
    }



    /** context **/

    public static String BUILD_PROPS = "build.props";
    public static String COMPONENT_BUILD_PROPS = "etc/build.props";
    public static String META_INFO = "<meta_info>";

    public static String TITLE = "title";
    public static String MODULE = "module";
    public static String NO_BUILD = "no_build";

    public static Context component_context(Path dir)
        throws IOException
    {
        Properties props = new Properties();
        Path props_path = dir.resolve(COMPONENT_BUILD_PROPS);
        props.load(Files.newBufferedReader(props_path));
        return new Context(dir, props, props_path.toString());
    }

    public static List<Context> component_contexts()
        throws IOException, InterruptedException
    {
        List<Context> result = new LinkedList<Context>();
        for (String p : Environment.getenv("ISABELLE_COMPONENTS").split(":", -1)) {
            if (!p.isEmpty()) {
                Path dir = Path.of(Environment.platform_path(p));
                if (Files.isRegularFile(dir.resolve(COMPONENT_BUILD_PROPS))) {
                    result.add(component_context(dir));
                }
            }
        }
        return List.copyOf(result);
    }

    public static class Context
    {
        private final Path _dir;
        private final Properties _props;
        private final String _location;

        public Context(Path dir, Properties props, String location)
        {
            _dir = dir;
            _props = props;
            _location = location;
        }

        @Override public String toString() { return _dir.toString(); }

        public String error_message(String msg)
        {
            if (_location == null || _location.isEmpty()) {
                return msg;
            }
            else {
                return msg +" (in " + Library.quote(_location) + ")";
            }
        }

        public boolean get_bool(String name) {
            String prop = _props.getProperty(name, "false");
            switch(prop) {
                case "true": return true;
                case "false": return false;
                default:
                    throw new RuntimeException(
                        error_message("Bad boolean property " + Library.quote(name) + ": " + Library.quote(prop)));
            }
        }

        public String title() { return _props.getProperty(TITLE, ""); }

        public String module_name() { return _props.getProperty(MODULE, ""); }
        public String module_result() { return get_bool(NO_BUILD) ? "" : module_name(); }

        public String scalac_options() { return _props.getProperty("scalac_options", ""); }
        public String javac_options() { return _props.getProperty("javac_options", ""); }
        public String main() { return _props.getProperty("main", ""); }

        private List<String> get_list(String name)
        {
            List<String> list = new LinkedList<String>();
            for (String s : _props.getProperty(name, "").split("\\s+")) {
                if (!s.isEmpty()) { list.add(s); }
            }
            return List.copyOf(list);
        }
        public List<String> requirements() { return get_list("requirements"); }
        public List<String> sources() { return get_list("sources"); }
        public List<String> resources() { return get_list("resources"); }
        public List<String> services() { return get_list("services"); }

        public boolean is_vacuous()
        {
            return sources().isEmpty() && resources().isEmpty() && services().isEmpty();
        }

        public Path path(String file)
            throws IOException, InterruptedException
        {
            return _dir.resolve(Environment.expand_platform_path(file));
        }

        public List<Path> requirement_paths(String s)
            throws IOException, InterruptedException
        {
            if (s.startsWith("env:")) {
                List<Path> paths = new LinkedList<Path>();
                for (String p : Environment.getenv(s.substring(4)).split(":", -1)) {
                    if (!p.isEmpty()) {
                        Path path = Path.of(Environment.platform_path(p));
                        paths.add(path);
                    }
                }
                return List.copyOf(paths);
            }
            else { return List.of(path(s)); }
        }

        public String item_name(String s)
        {
            int i = s.indexOf(':');
            return i > 0 ? s.substring(0, i) : s;
        }

        public String item_target(String s)
        {
            int i = s.indexOf(':');
            return i > 0 ? s.substring(i + 1) : s;
        }

        public String shasum(String name, List<Path> paths)
            throws NoSuchAlgorithmException, IOException, InterruptedException
        {
            return make_shasum(name, sha ->
                {
                    for (Path file : paths) {
                        if (Files.isRegularFile(file)) {
                            sha.update(Files.readAllBytes(file));
                        }
                        else {
                            throw new RuntimeException(
                                error_message("Bad input file " + Library.quote(file.toString())));
                        }
                    }
                });
        }

        public String shasum(String file)
            throws NoSuchAlgorithmException, IOException, InterruptedException
        {
            return shasum(file, List.of(path(file)));
        }

        public String shasum_meta_info()
            throws NoSuchAlgorithmException, IOException, InterruptedException
        {
            TreeMap<String,Object> sorted = new TreeMap<String,Object>();
            for (Object x : _props.entrySet()) {
                sorted.put(x.toString(), _props.get(x));
            }
            return make_shasum(META_INFO,
                    sha -> sha.update(sorted.toString().getBytes(StandardCharsets.UTF_8)));
        }
    }


    /** compile sources **/

    private static void add_options(List<String> options_list, String options)
    {
        if (options != null) {
            for (String s : options.split("\\s+")) {
                if (!s.isEmpty()) { options_list.add(s); }
            }
        }
    }

    public static void compile_scala_sources(
        PrintStream output,
        Path target_dir,
        String more_options,
        List<Path> deps,
        List<Path> sources) throws IOException, InterruptedException
    {
        ArrayList<String> args = new ArrayList<String>();
        add_options(args, Environment.getenv("ISABELLE_SCALAC_OPTIONS"));
        add_options(args, more_options);
        args.add("-d");
        args.add(target_dir.toString());
        args.add("-bootclasspath");
        args.add(Environment.join_platform_paths(deps));
        args.add("--");

        boolean scala_sources = false;
        for (Path p : sources) {
            args.add(p.toString());
            if (p.toString().endsWith(".scala")) { scala_sources = true; }
        }
        if (scala_sources) {
            String[] args_array = args.toArray(String[]::new);
            SimpleReporter reporter = new SimpleReporter() {
                @Override
                public void report(Diagnostic diagnostic) {
                    output.println(diagnostic.message());
                }
            };
            new Driver().process(args_array, reporter, null);
            ReporterResult result = new Driver().process(args_array);
            if (result.hasErrors()) { throw new RuntimeException("Failed to compile Scala sources"); }
        }
    }

    public static void compile_java_sources(
        PrintStream output,
        Path target_dir,
        String more_options,
        List<Path> deps,
        List<Path> sources) throws IOException, InterruptedException
    {
        JavaCompiler compiler = ToolProvider.getSystemJavaCompiler();
        StandardJavaFileManager file_manager =
            compiler.getStandardFileManager(null, Locale.ROOT, StandardCharsets.UTF_8);

        List<String> options = new LinkedList<String>();
        add_options(options, Environment.getenv("ISABELLE_JAVAC_OPTIONS"));
        add_options(options, more_options);
        options.add("-d");
        options.add(target_dir.toString());
        options.add("-classpath");
        options.add(Environment.join_platform_paths(deps));

        List<JavaFileObject> java_sources = new LinkedList<JavaFileObject>();
        for (Path p : sources) {
            if (p.toString().endsWith(".java")) {
                for (JavaFileObject o : file_manager.getJavaFileObjectsFromPaths(List.of(p))) {
                    java_sources.add(o);
                }
            }
        }

        if (!java_sources.isEmpty()) {
            CharArrayWriter out = new CharArrayWriter();
            boolean ok = compiler.getTask(out, file_manager, null, options, null, java_sources).call();
            out.flush();

            String msg = Library.trim_line(out.toString());
            if (ok) { if (!msg.isEmpty()) { output.print(msg + "\n"); } }
            else {
                throw new RuntimeException(
                    (msg.isEmpty() ? "" : msg + "\n") + "Failed to compile Java sources");
            }
        }
    }


    /** shasum for jar content **/

    private static String SHASUM = "META-INF/isabelle/shasum";

    public static String get_shasum(Path jar_path)
    {
        try {
            try (JarFile jar_file = new JarFile(jar_path.toFile()))
            {
                JarEntry entry = jar_file.getJarEntry(SHASUM);
                if (entry != null) {
                    byte[] bytes = jar_file.getInputStream(entry).readAllBytes();
                    return new String(bytes, StandardCharsets.UTF_8);
                }
                else { return ""; }
            }
        }
        catch (IOException exn) { return ""; }
    }

    public static void create_shasum(Path dir, String shasum)
        throws IOException
    {
        Path path = dir.resolve(SHASUM);
        Files.createDirectories(path.getParent());
        Files.writeString(path, shasum);
    }


    /** services **/

    private static String SERVICES = "META-INF/isabelle/services";

    public static List<String> get_services(Path jar_path)
        throws IOException
    {
        if (Files.isRegularFile(jar_path)) {
            try (JarFile jar_file = new JarFile(jar_path.toFile()))
            {
                JarEntry entry = jar_file.getJarEntry(SERVICES);
                if (entry != null) {
                    byte[] bytes = jar_file.getInputStream(entry).readAllBytes();
                    return Library.split_lines(new String(bytes, StandardCharsets.UTF_8));
                }
                else { return List.of(); }
            }
        }
        else { return List.of(); }
    }

    public static void create_services(Path dir, List<String> services)
        throws IOException
    {
        if (!services.isEmpty()) {
            Path path = dir.resolve(SERVICES);
            Files.createDirectories(path.getParent());
            Files.writeString(path, Library.cat_lines(services));
        }
    }


    /** create jar **/

    public static void create_jar(Path dir, String main, Path jar)
        throws IOException
    {
        Files.createDirectories(dir.resolve(jar).getParent());
        Files.deleteIfExists(jar);

        Manifest manifest = new Manifest();
        Attributes attributes = manifest.getMainAttributes();
        attributes.put(Attributes.Name.MANIFEST_VERSION, "1.0");
        attributes.put(new Attributes.Name("Created-By"),
            System.getProperty("java.version") + " (" + System.getProperty("java.vendor") + ")");
        if (!main.isEmpty()) { attributes.put(Attributes.Name.MAIN_CLASS, main); }

        try (JarOutputStream out =
            new JarOutputStream(new BufferedOutputStream(Files.newOutputStream(jar)), manifest))
        {
            for (Path path : Files.walk(dir).sorted().toArray(Path[]::new)) {
                boolean is_dir = Files.isDirectory(path);
                boolean is_file = Files.isRegularFile(path);
                if (is_dir || is_file) {
                    String name = Environment.slashes(dir.relativize(path).toString());
                    if (!name.isEmpty()) {
                        JarEntry entry = new JarEntry(is_dir ? name + "/" : name);
                        entry.setTime(path.toFile().lastModified());
                        out.putNextEntry(entry);
                        if (is_file) { out.write(Files.readAllBytes(path)); }
                        out.closeEntry();
                    }
                }
            }
        }
    }


    /** classpath **/

    public static List<Path> classpath()
        throws IOException, InterruptedException
    {
        List<Path> result = new LinkedList<Path>();
        for (Context context : component_contexts()) {
            String module = context.module_result();
            if (!module.isEmpty()) { result.add(context.path(module)); }
        }
        return List.copyOf(result);
    }

    public static List<String> services()
        throws IOException, InterruptedException
    {
        List<String> result = new LinkedList<String>();
        for (Context context : component_contexts()) {
            for (String s : context.services()) {
                result.add(s);
            }
        }
        return List.copyOf(result);
    }


    /** build **/

    public static void build(List<Path> classpath, PrintStream output, Context context, boolean fresh)
        throws NoSuchAlgorithmException, IOException, InterruptedException
    {
        String module = context.module_result();
        if (!module.isEmpty()) {
            String title = context.title();

            Path jar_path = context.path(module);
            String jar_name = jar_path.toString();
            if (!jar_name.endsWith(".jar")) {
                throw new RuntimeException(
                    context.error_message("Bad jar module " + Library.quote(jar_name)));
            }

            if (context.is_vacuous()) { Files.deleteIfExists(jar_path); }
            else {
                List<String> requirements = context.requirements();
                List<String> resources = context.resources();
                List<String> sources = context.sources();

                String shasum_old = get_shasum(jar_path);
                String shasum;
                List<Path> compiler_deps = new LinkedList<Path>();
                {
                    StringBuilder _shasum = new StringBuilder();
                    _shasum.append(context.shasum_meta_info());
                    for (String s : requirements) {
                        List<Path> paths = context.requirement_paths(s);
                        compiler_deps.addAll(paths);
                        _shasum.append(context.shasum(s, paths));
                    }
                    for (String s : resources) {
                        _shasum.append(context.shasum(context.item_name(s)));
                    }
                    for (String s : sources) { _shasum.append(context.shasum(s)); }
                    shasum = _shasum.toString();
                }
                if (fresh || !shasum_old.equals(shasum)) {
                    if (!title.isEmpty()) {
                        output.print("### Building " + title + " (" + jar_path + ") ...\n");
                    }

                    Path build_dir = Files.createTempDirectory("isabelle");
                    try {
                        /* compile sources */

                        compiler_deps.addAll(classpath);

                        List<Path> compiler_sources = new LinkedList<Path>();
                        for (String s : sources) { compiler_sources.add(context.path(s)); }

                        compile_scala_sources(output, build_dir,
                            context.scalac_options(), compiler_deps, compiler_sources);

                        compiler_deps.add(build_dir);
                        compile_java_sources(output, build_dir,
                            context.javac_options(), compiler_deps, compiler_sources);


                        /* copy resources */

                        for (String s : context.resources()) {
                            String name = context.item_name(s);
                            String target = context.item_target(s);
                            Path file_name = Path.of(name).normalize().getFileName();
                            Path target_path = Path.of(target).normalize();

                            Path target_dir;
                            Path target_file;
                            {
                                if (target.endsWith("/") || target.endsWith("/.")) {
                                    target_dir = build_dir.resolve(target_path);
                                    target_file = target_dir.resolve(file_name);
                                }
                                else {
                                    target_file = build_dir.resolve(target_path);
                                    target_dir = target_file.getParent();
                                }
                            }
                            Files.createDirectories(target_dir);
                            Files.copy(context.path(name), target_file,
                                StandardCopyOption.COPY_ATTRIBUTES);
                        }


                        /* packaging */

                        create_shasum(build_dir, shasum);
                        create_services(build_dir, context.services());
                        create_jar(build_dir, context.main(), jar_path);
                    }
                    finally {
                        try (Stream<Path> walk = Files.walk(build_dir)) {
                            walk.sorted(Comparator.reverseOrder())
                                .map(Path::toFile)
                                .forEach(File::delete);
                        }
                    }
                }
            }
        }
    }

    public static void build_components(PrintStream output, boolean fresh)
        throws NoSuchAlgorithmException, IOException, InterruptedException
    {
        List<Path> classpath = new LinkedList<Path>();
        for (String s : Environment.getenv("ISABELLE_CLASSPATH").split(":", -1)) {
            if (!s.isEmpty()) {
                classpath.add(Path.of(Environment.platform_path(s)));
            }
        }

        for (Context context : component_contexts()) {
            build(classpath, output, context, fresh);
        }
    }
}
