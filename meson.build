project('lispy', 'c',
  version : '0.1',
  default_options : ['warning_level=3'])

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required : true)

deps = [
  dependency('libedit'), 
  m_dep]
src = ['lispy.c', 'mpc.c']
executable('lispy', sources: src, dependencies: deps)
