vm = JNI::VirtualMachine.new

assert 'Call System.out.println' do
  env = vm.env
  sys = env.find_class "Ljava/lang/System;"
  f = env.static_field_id sys, "out", "Ljava/io/PrintStream;"
  out = env.static_object_field sys, f
  cls = env.object_class out
  println = env.method_id cls, "println", "(Ljava/lang/String;)V"
  assert_nil env.call_void_method out, println, env.new_string_utf("Hello World!")
end

assert 'Call Math.abs' do
  env = vm.env
  math = env.find_class "Ljava/lang/Math;"
  abs = env.static_method_id math, "abs", "(F)F"
  assert_equal 2.0, env.call_static_float_method(math, abs, -2.0)
end
