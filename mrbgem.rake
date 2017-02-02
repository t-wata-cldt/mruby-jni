MRuby::Gem::Specification.new 'mruby-jni' do |s|
  s.license = 'MIT'
  s.author = 'Takeshi Watanabe'
  s.summary = 'JNI binding for mruby'

  print "Getting Java Home\n"
  unless File.exists? "#{build_dir}/JavaHome.class"
    FileUtils.mkdir_p build_dir
    `javac -d #{build_dir} #{dir}/java_home.java`
  end
  jre_home = "#{`java -classpath #{build_dir} JavaHome`.strip}"
  java_home = "#{jre_home}/.."

  s.cc.include_paths << "#{java_home}/include"
  s.cc.include_paths << "#{java_home}/include/#{`uname -s`.strip.downcase}"
  s.linker.library_paths << "#{jre_home}/lib/server"
  s.linker.libraries << 'jvm'

  test_exec = exefile("#{s.build.build_dir}/bin/mrbtest")
  task :test => test_exec do
    `install_name_tool -delete_rpath #{jre_home}/lib/server #{test_exec}`
    sh "install_name_tool -add_rpath #{jre_home}/lib/server #{test_exec}"
  end if `uname -s`.strip == 'Darwin'
end
