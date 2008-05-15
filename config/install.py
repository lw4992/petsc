#!/usr/bin/env python
import re, os, sys, shutil

configDir = os.path.abspath('config')
sys.path.insert(0, configDir)
bsDir     = os.path.abspath(os.path.join(configDir, 'BuildSystem'))
sys.path.insert(0, bsDir)

import script

class Installer(script.Script):
  def __init__(self, clArgs = None):
    import RDict
    script.Script.__init__(self, clArgs, RDict.RDict())
    self.copies = []
    return

  def setupHelp(self, help):
    import nargs

    script.Script.setupHelp(self, help)
    help.addArgument('Installer', '-rootDir=<path>', nargs.Arg(None, None, 'Install Root Directory'))
    help.addArgument('Installer', '-installDir=<path>', nargs.Arg(None, None, 'Install Target Directory'))
    help.addArgument('Installer', '-arch=<type>', nargs.Arg(None, None, 'Architecture type'))
    help.addArgument('Installer', '-ranlib=<prog>', nargs.Arg(None, 'ranlib', 'Ranlib program'))
    help.addArgument('Installer', '-make=<prog>', nargs.Arg(None, 'make', 'Make program'))
    help.addArgument('Installer', '-libSuffix=<ext>', nargs.Arg(None, 'make', 'The static library suffix'))
    return

  def setupDirectories(self):
    self.rootDir    = os.path.abspath(self.argDB['rootDir'])
    self.installDir = os.path.abspath(self.argDB['installDir'])
    self.arch       = os.path.abspath(self.argDB['arch'])
    self.ranlib     = os.path.abspath(self.argDB['ranlib'])
    self.make       = os.path.abspath(self.argDB['make'])
    self.libSuffix  = os.path.abspath(self.argDB['libSuffix'])
    self.rootIncludeDir    = os.path.join(self.rootDir, 'include')
    self.archIncludeDir    = os.path.join(self.rootDir, self.arch, 'include')
    self.rootConfDir       = os.path.join(self.rootDir, 'conf')
    self.archConfDir       = os.path.join(self.rootDir, self.arch, 'conf')
    self.rootBinDir        = os.path.join(self.rootDir, 'bin')
    self.archBinDir        = os.path.join(self.rootDir, self.arch, 'bin')
    self.archLibDir        = os.path.join(self.rootDir, self.arch, 'lib')
    self.installIncludeDir = os.path.join(self.installDir, 'include')
    self.installConfDir    = os.path.join(self.installDir, 'conf')
    self.installLibDir     = os.path.join(self.installDir, 'lib')
    self.installBinDir     = os.path.join(self.installDir, 'bin')
    return

  def copytree(self, src, dst, symlinks = False, copyFunc = shutil.copy2):
    """Recursively copy a directory tree using copyFunc, which defaults to shutil.copy2().

    The destination directory must not already exist.
    If exception(s) occur, an shutil.Error is raised with a list of reasons.

    If the optional symlinks flag is true, symbolic links in the
    source tree result in symbolic links in the destination tree; if
    it is false, the contents of the files pointed to by symbolic
    links are copied.
    """
    copies = []
    names  = os.listdir(src)
    if not os.path.exists(dst):
      os.makedirs(dst)
    elif not os.path.isdir(dst):
      raise shutil.Error, 'Destination is not a directory'
    errors = []
    for name in names:
      srcname = os.path.join(src, name)
      dstname = os.path.join(dst, name)
      try:
        if symlinks and os.path.islink(srcname):
          linkto = os.readlink(srcname)
          os.symlink(linkto, dstname)
        elif os.path.isdir(srcname):
          copies.extend(self.copytree(srcname, dstname, symlinks))
        else:
          copyFunc(srcname, dstname)
          copies.append((srcname, dstname))
        # XXX What about devices, sockets etc.?
      except (IOError, os.error), why:
        errors.append((srcname, dstname, str(why)))
      # catch the Error from the recursive copytree so that we can
      # continue with other files
      except shutil.Error, err:
        errors.extend(err.args[0])
    try:
      shutil.copystat(src, dst)
    except WindowsError:
      # can't copy file access times on Windows
      pass
    except OSError, why:
      errors.extend((src, dst, str(why)))
    if errors:
      raise shutil.Error, errors
    return copies

  def installIncludes(self):
    self.copies.extend(self.copytree(self.rootIncludeDir, self.installIncludeDir))
    self.copies.extend(self.copytree(self.archIncludeDir, self.installIncludeDir))
    return

  def copyConf(self, src, dst):
    if os.path.isdir(dst):
      dst = os.path.join(dst, os.path.basename(src))
    lines   = []
    oldFile = open(src, 'r')
    for line in oldFile.readlines():
      lines.append(re.sub(self.rootDir, self.installDir, re.sub(os.path.join(self.rootDir, self.arch), self.installDir, line)))
    oldFile.close()
    newFile = open(dst, 'w')
    newFile.write(''.join(lines))
    newFile.close()
    shutil.copystat(src, dst)
    return

  def installConf(self):
    self.copies.extend(self.copytree(self.rootConfDir, self.installConfDir, copyFunc = self.copyConf))
    self.copies.extend(self.copytree(self.archConfDir, self.installConfDir, copyFunc = self.copyConf))
    return

  def installBin(self):
    self.copies.extend(self.copytree(self.rootBinDir, self.installBinDir))
    self.copies.extend(self.copytree(self.archBinDir, self.installBinDir))
    return

  def copyLib(self, src, dst):
    '''Run ranlib on the destination library if it is an archive'''
    shutil.copy2(src, dst)
    if os.path.splitext(dst)[1] == '.'+self.libSuffix:
      self.executeShellCommand(self.ranlib+' '+dst)
    return

  def installLib(self):
    self.copies.extend(self.copytree(self.archLibDir, self.installLibDir, copyFunc = self.copyLib))
    return

  def createUninstaller(self):
    f = open(os.path.join(self.installConfDir, 'uninstall.py'), 'w')
    # Could use the Python AST to do this
    f.write('copies = '+repr(self.copies))
    f.write('''
for src, dst in copies:
  os.remove(dst)
''')
    f.close()
    return

  def outputHelp(self):
    print '''
If using sh/bash, do the following:
  PETSC_DIR=%s; export PETSC_DIR
  unset PETSC_ARCH
If using csh/tcsh, do the following:
  setenv PETSC_DIR %s
  unsetenv PETSC_ARCH
Now run the testsuite to verify the install with the following:
  make test
''' % (self.installDir, self.installDir)
    return

  def run(self):
    self.setup()
    self.setupDirectories()
    if self.installDir == self.rootDir:
      print 'Install directory is current directory; nothing needs to be done'
      return
    print 'Installing PETSc at',self.installDir
    if not os.path.exists(self.installDir):
      os.makedirs(self.installDir)
    self.installIncludes()
    self.installConf()
    self.installBin()
    self.installLib()
    self.executeShellCommand(self.make+' PETSC_ARCH=""'+' PETSC_DIR='+self.installDir+' shared')
    self.createUninstaller()
    self.outputHelp()
    return

if __name__ == '__main__':
  Installer(sys.argv[1:]).run()
