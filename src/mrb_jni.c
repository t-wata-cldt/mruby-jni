#include <jni.h>

#include <mruby.h>
#include <mruby/class.h>
#include <mruby/data.h>
#include <mruby/string.h>


#define JNI_ERROR mrb_class_get(M, "JNIError")
#define DEFAULT_JNI_VERSION JNI_VERSION_1_6

#define check_error(ret, msg) \
  do { \
    if (ret != JNI_OK) { \
      mrb_raisef(M, JNI_ERROR, msg, mrb_fixnum_value(ret)); \
    } \
  } while(0) \

static void vm_free(mrb_state *M, void *ptr) {
  JavaVM* vm = (JavaVM*)ptr;
  (*vm)->DestroyJavaVM(vm);
}
static mrb_data_type const vm_type = { "JVM", vm_free };

static mrb_value vm_init(mrb_state *M, mrb_value self) {
  jint ret;

  JavaVMInitArgs args;
  args.version = DEFAULT_JNI_VERSION;

  ret = JNI_GetDefaultJavaVMInitArgs(&args);
  check_error(ret, "JVM argument initialization failed: %S");

  JavaVMOption options[] = {
  };

  args.nOptions = sizeof(options) / sizeof(options[0]);
  args.options = options;

  JavaVM *vm = NULL;
  JNIEnv *env = NULL;
  ret = JNI_CreateJavaVM(&vm, (void**)&env, &args);
  check_error(ret, "JVM creation error: %S");

  mrb_data_init(self, vm, &vm_type);

  return self;
}

static void jobject_free(mrb_state *M, void *ptr) {}
static mrb_data_type const jobject_type = { "JObject", jobject_free };

static void jclass_free(mrb_state *M, void *ptr) {}
static mrb_data_type const jclass_type = { "JClass", jclass_free };

static void jstring_free(mrb_state *M, void *ptr) {}
static mrb_data_type const jstring_type = { "JString", jstring_free };

static void jfield_id_free(mrb_state *M, void *ptr) {}
static mrb_data_type const jfield_id_type = { "JFieldID", jfield_id_free };

static void jmethod_id_free(mrb_state *M, void *ptr) {}
static mrb_data_type const jmethod_id_type = { "JMethodID", jmethod_id_free };

static void env_free(mrb_state *M, void *ptr) {}
static mrb_data_type const env_type = { "JNIEnv", env_free };

static mrb_value vm_env(mrb_state *M, mrb_value self) {
  JavaVM* vm = (JavaVM*)DATA_PTR(self);
  JNIEnv *env = NULL;

  jint const ret = (*vm)->GetEnv(vm, (void**)&env, DEFAULT_JNI_VERSION);
  check_error(ret, "Can't get JNIEnv: %S");

  return mrb_obj_value(mrb_data_object_alloc(
      M, mrb_class_get_under(M, mrb_module_get(M, "JNI"), "Env"), env, &env_type));
}

static mrb_value wrap_object(mrb_state *M, char const *cls, void* ptr, mrb_data_type const* t) {
  return mrb_obj_value(mrb_data_object_alloc(
      M, mrb_class_get_under(M, mrb_module_get(M, "JNI"), cls), ptr, t));
}

static mrb_value env_find_class(mrb_state *M, mrb_value self) {
  JNIEnv *env = (JNIEnv*)DATA_PTR(self);
  char const *name;
  mrb_get_args(M, "z", &name);

  return wrap_object(M, "JClass", (*env)->FindClass(env, name), &jclass_type);
}

static mrb_value env_static_field_id(mrb_state *M, mrb_value self) {
  JNIEnv *env = (JNIEnv*)DATA_PTR(self);
  jclass cls;
  char const *type, *name;
  mrb_get_args(M, "dzz", &cls, &jclass_type, &type, &name);

  return wrap_object(M, "JFieldID", (*env)->GetStaticFieldID(env, cls, type, name), &jfield_id_type);
}

static mrb_value env_static_object_field(mrb_state *M, mrb_value self) {
  JNIEnv *env = (JNIEnv*)DATA_PTR(self);
  jclass cls;
  jfieldID field_id;
  mrb_get_args(M, "dd", &cls, &jclass_type, &field_id, &jfield_id_type);

  return wrap_object(M, "JObject", (*env)->GetStaticObjectField(env, cls, field_id), &jobject_type);
}

static mrb_value env_object_class(mrb_state *M, mrb_value self) {
  JNIEnv *env = (JNIEnv*)DATA_PTR(self);
  jobject obj;
  mrb_get_args(M, "d", &obj, &jobject_type);

  return wrap_object(M, "JClass", (*env)->GetObjectClass(env, obj), &jclass_type);
}

static mrb_value env_method_id(mrb_state *M, mrb_value self) {
  JNIEnv *env = (JNIEnv*)DATA_PTR(self);
  jclass cls;
  char const *meth, *args;
  mrb_get_args(M, "dzz", &cls, &jclass_type, &meth, &args);

  return wrap_object(M, "JMethodID", (*env)->GetMethodID(env, cls, meth, args), &jmethod_id_type);
}

static mrb_value env_static_method_id(mrb_state *M, mrb_value self) {
  JNIEnv *env = (JNIEnv*)DATA_PTR(self);
  jclass cls;
  char const *meth, *args;
  mrb_get_args(M, "dzz", &cls, &jclass_type, &meth, &args);

  return wrap_object(M, "JMethodID", (*env)->GetStaticMethodID(env, cls, meth, args), &jmethod_id_type);
}

static mrb_value env_new_string_utf(mrb_state *M, mrb_value self) {
  JNIEnv *env = (JNIEnv*)DATA_PTR(self);
  char const *str;
  mrb_get_args(M, "z", &str);
  return wrap_object(M, "JString", (*env)->NewStringUTF(env, str), &jstring_type);
}

static jvalue to_jvalue(mrb_state *M, JNIEnv *env, mrb_value v) {
  jvalue ret;

  if (mrb_float_p(v)) {
    ret.f = (jfloat)mrb_float(v);
    return ret;
  }

  if (mrb_string_p(v)) {
    ret.l = (*env)->NewStringUTF(env, mrb_string_value_cstr(M, &v));
    return ret;
  }

  ret.l = mrb_data_check_get_ptr(M, v, &jstring_type);
  if (!ret.l) {
    ret.l = mrb_data_get_ptr(M, v, &jobject_type);
  }
  return ret;
}

static mrb_value env_call_void_method(mrb_state *M, mrb_value self) {
  JNIEnv *env = (JNIEnv*)DATA_PTR(self);
  jobject obj;
  jmethodID meth;
  mrb_value const *argv;
  mrb_int argc;
  mrb_get_args(M, "dd*", &obj, &jobject_type, &meth, &jmethod_id_type, &argv, &argc);

  jvalue jargs[argc];
  for (mrb_int i = 0; i < argc; ++i) {
    jargs[i] = to_jvalue(M, env, argv[i]);
  }

  return (*env)->CallVoidMethodA(env, obj, meth, jargs), mrb_nil_value();
}

static mrb_value env_call_float_method(mrb_state *M, mrb_value self) {
  JNIEnv *env = (JNIEnv*)DATA_PTR(self);
  jobject obj;
  jmethodID meth;
  mrb_value const *argv;
  mrb_int argc;
  mrb_get_args(M, "dd*", &obj, &jobject_type, &meth, &jmethod_id_type, &argv, &argc);

  jvalue jargs[argc];
  for (mrb_int i = 0; i < argc; ++i) {
    jargs[i] = to_jvalue(M, env, argv[i]);
  }

  return mrb_float_value(M, (*env)->CallFloatMethodA(env, obj, meth, jargs));
}

static mrb_value env_call_static_float_method(mrb_state *M, mrb_value self) {
  JNIEnv *env = (JNIEnv*)DATA_PTR(self);
  jobject obj;
  jmethodID meth;
  mrb_value const *argv;
  mrb_int argc;
  mrb_get_args(M, "dd*", &obj, &jclass_type, &meth, &jmethod_id_type, &argv, &argc);

  jvalue jargs[argc];
  for (mrb_int i = 0; i < argc; ++i) {
    jargs[i] = to_jvalue(M, env, argv[i]);
  }

  return mrb_float_value(M, (*env)->CallStaticFloatMethodA(env, obj, meth, jargs));
}

void mrb_mruby_jni_gem_init(mrb_state *M) {
  struct RClass *mod = mrb_define_module(M, "JNI");

  mrb_define_class(M, "JNIError", mrb_class_get(M, "StandardError"));

  struct RClass *vm = mrb_define_class_under(M, mod, "VirtualMachine", M->object_class);
  MRB_SET_INSTANCE_TT(vm, MRB_TT_DATA);
  mrb_define_method(M, vm, "initialize", vm_init, MRB_ARGS_NONE());
  mrb_define_method(M, vm, "env", vm_env, MRB_ARGS_NONE());

  struct RClass *env = mrb_define_class_under(M, mod, "Env", M->object_class);
  MRB_SET_INSTANCE_TT(env, MRB_TT_DATA);
  mrb_define_method(M, env, "find_class", env_find_class, MRB_ARGS_REQ(1));
  mrb_define_method(M, env, "static_field_id", env_static_field_id, MRB_ARGS_REQ(3));
  mrb_define_method(M, env, "static_object_field", env_static_object_field, MRB_ARGS_REQ(2));
  mrb_define_method(M, env, "object_class", env_object_class, MRB_ARGS_REQ(1));
  mrb_define_method(M, env, "method_id", env_method_id, MRB_ARGS_REQ(3));
  mrb_define_method(M, env, "static_method_id", env_static_method_id, MRB_ARGS_REQ(3));
  mrb_define_method(M, env, "new_string_utf", env_new_string_utf, MRB_ARGS_REQ(1));
  mrb_define_method(M, env, "call_void_method", env_call_void_method, MRB_ARGS_REQ(3));
  mrb_define_method(M, env, "call_float_method", env_call_float_method, MRB_ARGS_REQ(3));
  mrb_define_method(M, env, "call_static_float_method", env_call_static_float_method, MRB_ARGS_REQ(3));

  struct RClass *jobj = mrb_define_class_under(M, mod, "JObject", M->object_class);
  MRB_SET_INSTANCE_TT(jobj, MRB_TT_DATA);
  struct RClass *jcls = mrb_define_class_under(M, mod, "JClass", jobj);
  MRB_SET_INSTANCE_TT(jcls, MRB_TT_DATA);
  struct RClass *jstr = mrb_define_class_under(M, mod, "JString", jobj);
  MRB_SET_INSTANCE_TT(jstr, MRB_TT_DATA);
  struct RClass *jmethod_id = mrb_define_class_under(M, mod, "JMethodID", M->object_class);
  MRB_SET_INSTANCE_TT(jmethod_id, MRB_TT_DATA);
  struct RClass *jfield_id = mrb_define_class_under(M, mod, "JFieldID", M->object_class);
  MRB_SET_INSTANCE_TT(jfield_id, MRB_TT_DATA);
}

void mrb_mruby_jni_gem_final(mrb_state *M) {}
