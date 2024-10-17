#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <dlfcn.h>
#include <pthread.h>

#include <mach-o/dyld.h>
#include <CoreFoundation/CoreFoundation.h>

#include <jni.h>

typedef jint (JNICALL CreateJavaVM_t)(JavaVM **, void **, void *);

static char app_dir[PATH_MAX];

/* Dummy callback for the main thread loop. */
static void dummy_callback(void *info) { }

static char* get_application_directory(char *buffer, uint32_t len) {
    char *last_slash = NULL;
    int n = 2;

    if (! _NSGetExecutablePath(buffer, &len)) {
        while (n-- > 0) {
            if ((last_slash = strrchr(buffer, '/')))
                *last_slash = '\0';
        }
    }

    return last_slash ? buffer : NULL;
}

/* Execute the main method of our application. */
static int start_java_main(JNIEnv *env, char *main_class_name) {
    jclass main_class;
    jmethodID main_method;
    jobjectArray main_args;

    printf("looking for main class\n");
    if (!(main_class = (*env)->FindClass(env, main_class_name)))
        return -1;

    printf("looking for main method\n");
    if ( ! (main_method = (*env)->GetStaticMethodID(env, main_class, "main",
                                                    "([Ljava/lang/String;)V")) )
        return -1;

    printf("building args\n");
    main_args = (*env)->NewObjectArray(env, 0,
                                       (*env)->FindClass(env, "java/lang/String"),
                                       (*env)->NewStringUTF(env, ""));

    printf("Calling main method\n");
    (*env)->CallStaticVoidMethod(env, main_class, main_method, main_args);

    return 0;
}

void check_and_print_exception(JNIEnv *env) {
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
    }
}

/* Load and start the Java virtual machine. */
static void * start_jvm(void *arg) {
    char lib_path[PATH_MAX];
    void *lib;
    JavaVMInitArgs jvm_args;
    JavaVMOption jvm_opts[7];
    JavaVM *jvm;
    JNIEnv *env;
    CreateJavaVM_t *create_java_vm;

    (void) arg;

    /* Read the config file */
    FILE *ptr = fopen("Java/blubblaunch.config", "r");
    if (ptr == NULL)
        errx(EXIT_FAILURE, "Cannot read config file Java/blubblaunch.config\n");

    char mainclass[500];
    char mainjar[500];
    char javapath[500];
    if (fgets(mainclass, 500, ptr) == NULL)
        errx(EXIT_FAILURE, "Cannot read first line for mainclass\n");
    if (mainclass[strlen(mainclass) - 1] == '\n')
        mainclass[strlen(mainclass) - 1] = '\0';
    /* classes are usually provided with dots, but JNI needs slashes, so convert here */
    char *first_dot = NULL;
    while ((first_dot = strchr(mainclass, '.')))
        *first_dot = '/';
    printf("using mainclass: %s\n", mainclass);
    if (fgets(mainjar, 500, ptr) == NULL)
        errx(EXIT_FAILURE, "Cannot read second line for mainjar\n");
    if (mainjar[strlen(mainjar) - 1] == '\n')
        mainjar[strlen(mainjar) - 1] = '\0';
    if (fgets(javapath, 500, ptr) == NULL)
        errx(EXIT_FAILURE, "Cannot read third line for path to Java home, relative to Contents\n");
    if (javapath[strlen(javapath) - 1] == '\n')
        javapath[strlen(javapath) - 1] = '\0';

    /* Load the Java library in the bundled JRE. */
    snprintf(lib_path, PATH_MAX, "%s/%s/lib/libjli.dylib", app_dir, javapath);
    if (!(lib = dlopen(lib_path, RTLD_LAZY))) {
        printf("could not open %s, trying subdir lib/ for older Java\n", lib_path);
        snprintf(lib_path, PATH_MAX, "%s/%s/lib/jli/libjli.dylib", app_dir, javapath);
        if (!(lib = dlopen(lib_path, RTLD_LAZY)))
            errx(EXIT_FAILURE, "Cannot load Java library: %s", dlerror());
    }

    printf("creating Java VM using JRE at %s\n", lib_path);
    if (!(create_java_vm = (CreateJavaVM_t *)dlsym(lib, "JNI_CreateJavaVM")))
        errx(EXIT_FAILURE, "Cannot find JNI_CreateJavaVM: %s", dlerror());

    /* Prepare options for the JVM. */
    char class_path[550];
    snprintf(class_path, 550, "-Djava.class.path=Java/%s", mainjar);
    printf("class path: %s\n", class_path);
    jvm_opts[0].optionString = class_path;
    jvm_opts[1].optionString = "-Xmx2g";
    jvm_opts[2].optionString = "-Xms400M";
    jvm_opts[3].optionString = "-Dcom.apple.smallTabs=true";
    jvm_opts[4].optionString = "-Dapple.laf.useScreenMenuBar=true";
    jvm_opts[5].optionString = "-Dcom.apple.macos.use-file-dialog-packages=true";
    jvm_opts[6].optionString = "-Dcom.apple.macos.useScreenMenuBar=true";
    jvm_args.version = JNI_VERSION_1_2;
    jvm_args.ignoreUnrecognized = JNI_TRUE;
    jvm_args.options = jvm_opts;
    jvm_args.nOptions = 7;

    if ( create_java_vm(&jvm, (void **)&env, &jvm_args) == JNI_ERR ) {
        check_and_print_exception(env);
        errx(EXIT_FAILURE, "Cannot create Java virtual machine");
    }

    if ( start_java_main(env, mainclass) != 0 ) {
        check_and_print_exception(env);
        (*jvm)->DestroyJavaVM(jvm);
        errx(EXIT_FAILURE, "Cannot start Java main method");
    }

    if ( (*env)->ExceptionCheck(env) ) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
    }

    (*jvm)->DetachCurrentThread(jvm);
    (*jvm)->DestroyJavaVM(jvm);

    /* Calling exit() here will terminate both this JVM thread and the
     * infinite loop in the main thread. */
    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    pthread_t jvm_thread;
    pthread_attr_t jvm_thread_attr;
    CFRunLoopSourceContext loop_context;
    CFRunLoopSourceRef loop_ref;

    (void) argc;
    (void) argv;

    if ( ! get_application_directory(app_dir, PATH_MAX) )
        errx(EXIT_FAILURE, "Cannot get application directory");

    if ( chdir(app_dir) == -1 )
        err(EXIT_FAILURE, "Cannot change current directory");

    printf("working dir: %s\n", app_dir);

    /* Start the thread where the JVM will run. */
    pthread_attr_init(&jvm_thread_attr);
    pthread_attr_setscope(&jvm_thread_attr, PTHREAD_SCOPE_SYSTEM);
    pthread_attr_setdetachstate(&jvm_thread_attr, PTHREAD_CREATE_DETACHED);
    if ( pthread_create(&jvm_thread, &jvm_thread_attr, start_jvm, NULL) != 0 )
        err(EXIT_FAILURE, "Cannot start JVM thread");
    pthread_attr_destroy(&jvm_thread_attr);

    /* Run a dummy loop in the main thread. */
    memset(&loop_context, 0, sizeof(loop_context));
    loop_context.perform = &dummy_callback;
    loop_ref = CFRunLoopSourceCreate(NULL, 0, &loop_context);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), loop_ref, kCFRunLoopCommonModes);
    CFRunLoopRun();

    return EXIT_SUCCESS;
}
