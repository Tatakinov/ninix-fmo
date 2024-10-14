require 'mkmf'

func = []
if RbConfig::CONFIG['host_os'] =~ /mswin|mingw|cygwin/
  func = ['CreateMutex', 'OpenMutex', 'CreateFileMapping', 'OpenFileMapping',
          'WaitForSingleObject', 'MapViewOfFile', 'UnmapViewOfFile',
          'CloseHandle']
else
  func = ['strndup', 'shm_open', 'ftruncate', 'mmap', 'sem_init', 'sem_wait', 'sem_post',
          'munmap', 'shm_unlink', 'close']
end
for f in func
  abort 'missing ' + f unless have_func f
end

create_makefile 'ninix_fmo/ninix_fmo'
