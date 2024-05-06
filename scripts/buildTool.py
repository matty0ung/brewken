#!/usr/bin/env python3
#-----------------------------------------------------------------------------------------------------------------------
# scripts/buildTool.py is part of Brewken, and is copyright the following authors 2022-2024:
#   • Matt Young <mfsy@yahoo.com>
#
# Brewken is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later
# version.
#
# Brewken is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
# warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with this program.  If not, see
# <http://www.gnu.org/licenses/>.
#-----------------------------------------------------------------------------------------------------------------------

#-----------------------------------------------------------------------------------------------------------------------
# This Python script is intended to be invoked by the `bt` bash script in the parent directory.  See comments in that
# script for why.
#
# .:TODO:. We should probably also break this file up into several smaller ones!
#
# Note that Python allows both single and double quotes for delimiting strings.  In Meson, we need single quotes, in
# C++, we need double quotes.  We mostly try to use single quotes below for consistency with Meson, except where using
# double quotes avoids having to escape a single quote.
#-----------------------------------------------------------------------------------------------------------------------


#-----------------------------------------------------------------------------------------------------------------------
# Python built-in modules we use
#-----------------------------------------------------------------------------------------------------------------------
import argparse
import datetime
import glob
import logging
import os
import pathlib
import platform
import re
import shutil
import stat
import subprocess
import sys
import tempfile
from decimal import Decimal

#-----------------------------------------------------------------------------------------------------------------------
# Our own modules
#-----------------------------------------------------------------------------------------------------------------------
import btUtils

#-----------------------------------------------------------------------------------------------------------------------
# Global constants
#-----------------------------------------------------------------------------------------------------------------------
# There is some inevitable duplication with constants in meson.build, but we try to keep it to a minimum
projectName = 'brewken'
capitalisedProjectName = projectName.capitalize()
projectUrl = 'https://github.com/' + capitalisedProjectName + '/' + projectName + '/'

# By default we'll log at logging.INFO, but this can be overridden via the -v and -q command line options -- see below
log = btUtils.getLogger()

exe_python = shutil.which('python3')

#-----------------------------------------------------------------------------------------------------------------------
# Welcome banner and environment info
#-----------------------------------------------------------------------------------------------------------------------
# The '%c' argument to strftime means "Locale’s appropriate date and time representation"
log.info(
   '⭐ ' + capitalisedProjectName + ' Build Tool (bt), invoked as "' + ' '.join(sys.argv) + '" starting run on ' +
   platform.system() + ' (' + platform.release() + '), using Python ' + platform.python_version() + ' from ' +
   exe_python + ', with command line arguments, at ' + datetime.datetime.now().strftime('%c') + ' ⭐'
)

# This is long but it can be a useful diagnostic
log.info('Environment variables vvv')
envVars = dict(os.environ)
for key, value in envVars.items():
   log.info('Env: ' + key + '=' + value)
log.info('Environment variables ^^^')

# Since (courtesy of the 'bt' script that invokes us) we're running in a venv, the pip we find should be the one in the
# venv.  This means that it will install to the venv and not mind about external package managers.
exe_pip = shutil.which('pip3')
# If Pip still isn't installed we need to bail here.
if (exe_pip is None or exe_pip == ''):
   pathEnvVar = ''
   if ('PATH' in os.environ):
      pathEnvVar = os.environ['PATH']
   log.critical(
      'Cannot find pip (PATH=' + pathEnvVar + ') - please see https://pip.pypa.io/en/stable/installation/ for how to ' +
      'install'
   )
   exit(1)

log.info('Found pip at: ' + exe_pip)

# Of course, when you run the pip in the venv, it might complain that it is not up-to-date.  So we should ensure that
# first.
btUtils.abortOnRunFail(subprocess.run([exe_pip, 'install', '--upgrade', 'pip']))

#
# We use the packaging module (see https://pypi.org/project/packaging/) for handling version numbers (as described at
# https://packaging.pypa.io/en/stable/version.html).
#
# On some platforms, we also need to install setuptools to be able to access packaging.version.  (NB: On MacOS,
# setuptools is now installed by default by Homebrew when it installs Python, so we'd get an error if we try to install
# it via pip here.)
#
log.info('pip install packaging')
btUtils.abortOnRunFail(subprocess.run([exe_pip, 'install', 'packaging']))
log.info('pip install setuptools')
btUtils.abortOnRunFail(subprocess.run([exe_pip, 'install', 'setuptools']))
import packaging.version

# The requests library (see https://pypi.org/project/requests/) is used for downloading files in a more Pythonic way
# than invoking wget through the shell.
log.info('pip install requests')
btUtils.abortOnRunFail(subprocess.run([exe_pip, 'install', 'requests']))
import requests

#
# Once all platforms we're running on have Python version 3.11 or above, we will be able to use the built-in tomllib
# library (see https://docs.python.org/3/library/tomllib.html) for parsing TOML.  Until then, it's easier to import the
# tomlkit library (see https://pypi.org/project/tomlkit/) which actually has rather more functionality than we need
#
btUtils.abortOnRunFail(subprocess.run([exe_pip, 'install', 'tomlkit']))
import tomlkit

#-----------------------------------------------------------------------------------------------------------------------
# Parse command line arguments
#-----------------------------------------------------------------------------------------------------------------------
# We do this (nearly) first as we want the program to exit straight away if incorrect arguments are specified
# Choosing which action to call is done a the end of the script, after all functions are defined
#
# Using Python argparse saves us writing a lot of boilerplate, although the help text it generates on the command line
# is perhaps a bit more than we want (eg having to separate 'bt --help' and 'bt setup --help' is overkill for us).
# There are ways around this -- eg see
# https://stackoverflow.com/questions/20094215/argparse-subparser-monolithic-help-output -- but they are probably more
# complexity than is merited here.
#
parser = argparse.ArgumentParser(
   prog = 'bt',
   description = capitalisedProjectName + ' build tool.  A utility to help with installing dependencies, Git ' +
                 'setup, Meson build configuration and packaging.',
   epilog = 'See ' + projectUrl + ' for info and latest releases'
)

# Log level
group = parser.add_mutually_exclusive_group()
group.add_argument('-v', '--verbose', action = 'store_true', help = 'Enable debug logging of this script')
group.add_argument('-q', '--quiet',   action = 'store_true', help = 'Suppress info logging of this script')

# Per https://docs.python.org/3/library/argparse.html#sub-commands, you use sub-parsers for sub-commands.
subparsers = parser.add_subparsers(
   dest = 'subCommand',
   required = True,
   title = 'action',
   description = "Exactly one of the following actions must be specified.  (For actions marked ✴, specify -h or "
                 "--help AFTER the action for info about options -- eg '%(prog)s setup --help'.)"
)

# Parser for 'setup'
parser_setup = subparsers.add_parser('setup', help = '✴ Set up meson build directory (mbuild) and git options')
subparsers_setup = parser_setup.add_subparsers(dest = 'setupOption', required = False)
parser_setup_all = subparsers_setup.add_parser(
   'all',
   help = 'Specifying this will also automatically install libraries and frameworks we depend on'
)

# Parser for 'package'
parser_package = subparsers.add_parser('package', help='Build a distributable installer')

#
# Process the arguments for use below
#
# This try/expect ensures that help is printed if the script is invoked without arguments.  It's not perfect as you get
# the usage line twice (because parser.parse_args() outputs it to stderr before throwing SystemExit) but it's good
# enough for now at least.
#
try:
   args = parser.parse_args()
except SystemExit as se:
   if (se.code != None and se.code != 0):
      parser.print_help()
   sys.exit(0)

#
# The one thing we do set straight away is log level
# Possible levels are 'CRITICAL', 'ERROR', 'WARNING', 'INFO', 'DEBUG', 'NOTSET'.  We choose 'INFO' for default, 'DEBUG'
# for verbose and 'WARNING' for quiet.  You wouldn't want to suppress warnings, would you? :-)
#
if (args.verbose):
   log.setLevel(logging.DEBUG)
elif (args.quiet):
   log.setLevel(logging.WARNING)

log.debug('Parsed command line arguments as ' + str(args))

#-----------------------------------------------------------------------------------------------------------------------
# Note the working directory from which we were invoked -- though it shouldn't matter as we try to be independent of
# this
#-----------------------------------------------------------------------------------------------------------------------
log.debug('Working directory when invoked: ' + pathlib.Path.cwd().as_posix())

#-----------------------------------------------------------------------------------------------------------------------
# Standard Directories
#-----------------------------------------------------------------------------------------------------------------------
dir_base          = btUtils.getBaseDir()
dir_gitInfo       = dir_base.joinpath('.git')
dir_build         = dir_base.joinpath('mbuild')
# Where submodules live and how many there are.  Currently there are 2: libbacktrace and valijson
dir_gitSubmodules = dir_base.joinpath('third-party')
num_gitSubmodules = 2
# Top-level packaging directory - NB deliberately different name from 'packaging' (= dir_base.joinpath('packaging'))
dir_packages          = dir_build.joinpath('packages')
dir_packages_platform = dir_packages.joinpath(platform.system().lower())   # Platform-specific packaging directory
dir_packages_source   = dir_packages.joinpath('source')

#-----------------------------------------------------------------------------------------------------------------------
# Helper function for copying one or more files to a directory that might not yet exist
#-----------------------------------------------------------------------------------------------------------------------
def copyFilesToDir(files, directory):
   os.makedirs(directory, exist_ok=True)
   for currentFile in files:
      log.debug('Copying ' + currentFile + ' to ' + directory)
      shutil.copy2(currentFile, directory)
   return

#-----------------------------------------------------------------------------------------------------------------------
# Helper function for counting files in a directory tree
#-----------------------------------------------------------------------------------------------------------------------
def numFilesInTree(path):
   numFiles = 0
   for root, dirs, files in os.walk(path):
      numFiles += len(files)
   return numFiles

#-----------------------------------------------------------------------------------------------------------------------
# Helper function for downloading a file
#-----------------------------------------------------------------------------------------------------------------------
def downloadFile(url):
   filename = url.split('/')[-1]
   log.info('Downloading ' + url + ' to ' + filename + ' in directory ' + pathlib.Path.cwd().as_posix())
   response = requests.get(url)
   if (response.status_code != 200):
      log.critical('Error code ' + response.status_code + ' while downloading ' + url)
      exit(1)
   with open(filename, 'wb') as fd:
      for chunk in response.iter_content(chunk_size = 128):
         fd.write(chunk)
   return

#-----------------------------------------------------------------------------------------------------------------------
# Set global variables exe_git and exe_meson with the locations of the git and meson executables plus mesonVersion with
# the version of meson installed
#
# We want to give helpful error messages if Meson or Git is not installed.  For other missing dependencies we can rely
# on Meson itself to give explanatory error messages.
#-----------------------------------------------------------------------------------------------------------------------
def findMesonAndGit():
   # Advice at https://docs.python.org/3/library/subprocess.html is "For maximum reliability, use a fully qualified path
   # for the executable. To search for an unqualified name on PATH, use shutil.which()"

   # Check Meson is installed.  (See installDependencies() below for what we do to attempt to install it from this
   # script.)
   global exe_meson
   exe_meson = shutil.which("meson")
   if (exe_meson is None or exe_meson == ""):
      log.critical('Cannot find meson - please see https://mesonbuild.com/Getting-meson.html for how to install')
      exit(1)

   global mesonVersion
   rawVersion = btUtils.abortOnRunFail(subprocess.run([exe_meson, '--version'], capture_output=True)).stdout.decode('UTF-8').rstrip()
   log.debug('Meson version raw: ' + rawVersion)
   mesonVersion = packaging.version.parse(rawVersion)
   log.debug('Meson version parsed: ' + str(mesonVersion))

   # Check Git is installed if its magic directory is present
   global exe_git
   exe_git   = shutil.which("git")
   if (dir_gitInfo.is_dir()):
      log.debug('Found git information directory:' + dir_gitInfo.as_posix())
      if (exe_git is None or exe_git == ""):
         log.critical('Cannot find git - please see https://git-scm.com/downloads for how to install')
         exit(1)

   return

#-----------------------------------------------------------------------------------------------------------------------
# Copy a file, removing comments and folded lines
#
# Have had various problems with comments in debian package control file, even though they are theoretically allowed, so
# we strip them out here, hence slightly more involved code than just
#    shutil.copy2(dir_build.joinpath('control'), dir_packages_deb_control)
#
# Similarly, some of the fields in the debian control file that we want to split across multiple lines are not actually
# allowed to be so "folded" by the Debian package generator.  So, we do our own folding here.  (At the same time, we
# remove extra spaces that make sense on the unfolded line but not once everything is joined onto single line.)
#-----------------------------------------------------------------------------------------------------------------------
def copyWithoutCommentsOrFolds(inputPath, outputPath):
   with open(inputPath, 'r') as inputFile, open(outputPath, 'w') as outputFile:
      for line in inputFile:
         if (not line.startswith('#')):
            if (not line.endswith('\\\n')):
               outputFile.write(line)
            else:
               foldedLine = ""
               while (line.endswith('\\\n')):
                  foldedLine += line.removesuffix('\\\n')
                  line = next(inputFile)
               foldedLine += line
               # The split and join here is a handy trick for removing repeated spaces from the line without
               # fumbling around with regular expressions.  Note that this takes the newline off the end, hence
               # why we have to add it back manually.
               outputFile.write(' '.join(foldedLine.split()))
               outputFile.write('\n')
   return

#-----------------------------------------------------------------------------------------------------------------------
# Create fileToDistribute.sha256sum for a given fileToDistribute in a given directory
#-----------------------------------------------------------------------------------------------------------------------
def writeSha256sum(directory, fileToDistribute):
   #
   # In Python 3.11 we could use the file_digest() function from the hashlib module to do this.  But it's rather
   # more work to do in Python 3.10, so we just use the `sha256sum` command instead.
   #
   # Note however, that `sha256sum` includes the supplied directory path of a file in its output.  We want just the
   # filename, not its full or partial path on the build machine.  So we change into the directory of the file before
   # running the `sha256sum` command.
   #
   previousWorkingDirectory = pathlib.Path.cwd().as_posix()
   os.chdir(directory)
   with open(directory.joinpath(fileToDistribute + '.sha256sum').as_posix(),'w') as sha256File:
      btUtils.abortOnRunFail(
         subprocess.run(['sha256sum', fileToDistribute],
                        capture_output=False,
                        stdout=sha256File)
      )
   os.chdir(previousWorkingDirectory)
   return

#-----------------------------------------------------------------------------------------------------------------------
# Ensure git submodules are present
#
# When a git repository is cloned, the submodules don't get cloned until you specifically ask for it via the
# --recurse-submodules flag.
#
# (Adding submodules is done via Git itself.  Eg:
#    cd ../third-party
#    git submodule add https://github.com/ianlancetaylor/libbacktrace
# But this only needs to be done once, by one person, and committed to our repository, where the connection is
# stored in the .gitmodules file.)
#-----------------------------------------------------------------------------------------------------------------------
def ensureSubmodulesPresent():
   findMesonAndGit()
   if (not dir_gitSubmodules.is_dir()):
      log.info('Creating submodules directory: ' + dir_gitSubmodules.as_posix())
      os.makedirs(dir_gitSubmodules, exist_ok=True)
   if (numFilesInTree(dir_gitSubmodules) < num_gitSubmodules):
      log.info('Pulling in submodules in ' + dir_gitSubmodules.as_posix())
      btUtils.abortOnRunFail(subprocess.run([exe_git, "submodule", "init"], capture_output=False))
      btUtils.abortOnRunFail(subprocess.run([exe_git, "submodule", "update"], capture_output=False))
   return

#-----------------------------------------------------------------------------------------------------------------------
# Function to install dependencies -- called if the user runs 'bt setup all'
#-----------------------------------------------------------------------------------------------------------------------
def installDependencies():
   log.info('Checking which dependencies need to be installed')
   #
   # I looked at using ConanCenter (https://conan.io/center/) as a source of libraries, so that we could automate
   # installing dependencies, but it does not have all the ones we need.  Eg it has Boost, Qt, Xerces-C and Valijson,
   # but not Xalan-C.  (Someone else has already requested Xalan-C, see
   # https://github.com/conan-io/conan-center-index/issues/5546, but that request has been open a long time, so its
   # fulfilment doesn't seem imminent.)  It also doesn't yet integrate quite as well with meson as we might like (eg
   # as at 2023-01-15, https://docs.conan.io/en/latest/reference/conanfile/tools/meson.html is listed as "experimental
   # and subject to breaking changes".
   #
   # Another option is vcpkg (https://vcpkg.io/en/index.html), which does have both Xerces-C and Xalan-C, along with
   # Boost, Qt and Valijson.  There is an example here https://github.com/Neumann-A/meson-vcpkg of how to use vcpkg from
   # Meson.  However, it's pretty slow to get started with because it builds from source everything it installs
   # (including tools it depends on such as CMake) -- even if they are already installed on your system from another
   # source.  This is laudably correct but I'm too impatient to do things that way.
   #
   # Will probably take another look at Conan in future, subject to working out how to have it use already-installed
   # versions of libraries/frameworks if they are present.  The recommended way to install Conan is via a Python
   # package, which makes that part easy.  However, there is a fair bit of other ramp-up to do, and some breaking
   # changes between "current" Conan 1.X and "soon-to-be-released" Conan 2.0.  So, will leave it for now and stick
   # mostly to native installs for each of the 3 platforms (Linux, Windows, Mac).
   #
   # Other notes:
   #    - GNU coreutils (https://www.gnu.org/software/coreutils/manual/coreutils.html) is probably already installed on
   #      most Linux distros, but not necessarily on Mac and Windows.  It gives us sha256sum.
   #
   match platform.system():

      #-----------------------------------------------------------------------------------------------------------------
      #---------------------------------------------- Linux Dependencies -----------------------------------------------
      #-----------------------------------------------------------------------------------------------------------------
      case 'Linux':
         #
         # NOTE: For the moment at least, we are assuming you are on Ubuntu or another Debian-based Linux.  For other
         # flavours of the OS you need to install libraries and frameworks manually.
         #

         #
         # For almost everything apart form Boost (see below) we can rely on the distro packages.  A few notes:
         #  - We need CMake even for the Meson build because meson uses CMake as one of its library-finding tools
         #  - The pandoc package helps us create man pages from markdown input
         #  - The build-essential and debhelper packages are for creating Debian packages
         #  - The rpm and rpmlint packages are for creating RPM packages
         #  - We need python-dev to build parts of Boost -- though it may be we could do without this as we only use a
         #    few parts of Boost and most Boost libraries are header-only, so do not require compilation.
         #
         log.info('Ensuring libraries and frameworks are installed')
         btUtils.abortOnRunFail(subprocess.run(['sudo', 'apt-get', 'update']))
         btUtils.abortOnRunFail(
            subprocess.run(
               ['sudo', 'apt', 'install', '-y', 'build-essential',
                                                'cmake',
                                                'coreutils',
                                                'debhelper',
                                                'git',
                                                'libqt5multimedia5-plugins',
                                                'libqt5sql5-psql',
                                                'libqt5sql5-sqlite',
                                                'libqt5svg5-dev',
                                                'libxalan-c-dev',
                                                'libxerces-c-dev',
                                                'lintian',
                                                'meson',
                                                'ninja-build',
                                                'pandoc',
                                                'python3',
                                                'python3-dev',
                                                'qtbase5-dev',
                                                'qtmultimedia5-dev',
                                                'qttools5-dev',
                                                'qttools5-dev-tools',
                                                'rpm',
                                                'rpmlint']
            )
         )

         #
         # Thanks to the build-essential package (installed if necessary above), we know there is now _some_ version of
         # g++ on the system.  We actually need g++ 10 or newer because g++ 9 does not includes the <concepts> header.
         # So, on older releases (eg Ubuntu 20.04), we need to install a newer g++ (which will sit alongside the system
         # default one).
         #
         minGppVersion = packaging.version.parse('10.1.0')
         gppVersionOutput = btUtils.abortOnRunFail(
            subprocess.run(['g++', '--version'], encoding = "utf-8", capture_output = True)
         )
         # We only want the first line of the output from `g++ --version`.  The rest is just the copyright notice.
         gppVersionLine = str(gppVersionOutput.stdout).split('\n', 1)[0]
         log.debug('"g++ --version" returned ' + str(gppVersionOutput.stdout))
         # The version line will be something along the lines of "g++ (Ubuntu 11.4.0-1ubuntu1~22.04) 11.4.0", so we
         # split on spaces and take the last field.
         gppVersionRaw = gppVersionLine.split(' ')[-1]
         gppVersionFound = packaging.version.parse(gppVersionRaw)
         log.debug('Parsed as ' + str(gppVersionFound) + '.')
         if (gppVersionFound < minGppVersion):
            log.info('Installing gcc/g++ 10 as current version is ' + str(gppVersionFound))
            btUtils.abortOnRunFail(subprocess.run(['sudo', 'apt', 'install', '-y', 'gcc-10', 'g++-10']))
            #
            # Now we have to tell the system to use the version 10 compiler by default.  This is a little bit high-
            # handed, but we need a way for the automated "old Linux" build to work and I can't find another way to make
            # both Meson and CMake use the version 10 compiler rather than the system default one.
            #
            # This is relatively easily reversible for anyone setting up a local build.
            #
            log.info('Running "update-alternatives" command to set gcc/g++ 10 as default compiler')
            btUtils.abortOnRunFail(subprocess.run([
               'sudo', 'update-alternatives', '--install', '/usr/bin/gcc', 'gcc', '/usr/bin/gcc-10', '60', '--slave', '/usr/bin/g++', 'g++', '/usr/bin/g++-10'
            ]))

         #
         # We need a recent version of Boost, ie Boost 1.79 or newer, to use Boost.JSON.  For Windows and Mac this is
         # fine if you are installing from MSYS2 (https://packages.msys2.org/package/mingw-w64-x86_64-boost) or
         # Homebrew (https://formulae.brew.sh/formula/boost) respectively.  Unfortunately, as of late-2022, many
         # Linux distros provide only older versions of Boost.  (Eg, on Ubuntu, you can see this by running
         # 'apt-cache policy libboost-dev'.)
         #
         # First, check whether Boost is installed and if so, what version
         #
         # We'll look in the following places:
         #    /usr/include/boost/version.hpp        <-- Distro-installed Boost
         #    /usr/local/include/boost/version.hpp  <-- Manually-installed Boost
         #    ${BOOST_ROOT}/boost/version.hpp       <-- If the BOOST_ROOT environment variable is set it gives an
         #                                              alternative place to look
         #
         # Although things should compile with 1.79.0, if we're going to all the bother of installing Boost manually,
         # we'll install a more recent one.
         minBoostVersion = packaging.version.parse('1.79.0')
         boostVersionToInstall = packaging.version.parse('1.84.0') # NB: This _must_ have the patch version
         maxBoostVersionFound = packaging.version.parse('0')
         possibleBoostVersionHeaders = [pathlib.Path('/usr/include/boost/version.hpp'),
                                        pathlib.Path('/usr/local/include/boost/version.hpp')]
         if ('BOOST_ROOT' in os.environ):
            possibleBoostVersionHeaders.append(pathlib.Path(os.environ['BOOST_ROOT']).joinpath('boost/version.hpp'))
         for boostVersionHeader in possibleBoostVersionHeaders:
            if (boostVersionHeader.is_file()):
               runResult = btUtils.abortOnRunFail(
                  subprocess.run(
                     ['grep', '#define BOOST_LIB_VERSION ', boostVersionHeader.as_posix()],
                     encoding = "utf-8",
                     capture_output = True
                  )
               )
               log.debug('In ' + boostVersionHeader.as_posix() + ' found ' + str(runResult.stdout))
               versionFoundRaw = re.sub(
                  r'^.*BOOST_LIB_VERSION "([0-9_]*)".*$', r'\1', str(runResult.stdout).rstrip()
               ).replace('_', '.')
               versionFound = packaging.version.parse(versionFoundRaw)
               if (versionFound > maxBoostVersionFound):
                  maxBoostVersionFound = versionFound
               log.debug('Parsed as ' + str(versionFound) + '.  (Highest found = ' + str(maxBoostVersionFound) + ')')

         #
         # The Boost version.hpp configuration header file gives two constants for defining the version of Boost
         # installed:
         #
         # BOOST_VERSION is a pure numeric value:
         #    BOOST_VERSION % 100 is the patch level
         #    BOOST_VERSION / 100 % 1000 is the minor version
         #    BOOST_VERSION / 100000 is the major version
         # So, eg, for Boost 1.79.0 (= 1.079.00), BOOST_VERSION = 107900
         #
         # BOOST_LIB_VERSION is a string value with underscores instead of dots (and without the patch level if that's
         # 0).  So, eg for Boost 1.79.0, BOOST_LIB_VERSION = "1_79" (and for 1.23.45 it would be "1_23_45")
         #
         # We use BOOST_LIB_VERSION as it's easy to convert it to a version number that Python can understand
         #
         log.debug(
            'Max version of Boost found: ' + str(maxBoostVersionFound) + '.  Need >= ' + str(minBoostVersion) +
            ', otherwise will try to install ' + str(boostVersionToInstall)
         )
         if (maxBoostVersionFound < minBoostVersion):
            log.info(
               'Installing Boost ' + str(boostVersionToInstall) + ' as newest version found was ' +
               str(maxBoostVersionFound)
            )
            #
            # To manually install the latest version of Boost from source, first we uninstall any old version
            # installed via the distro (eg, on Ubuntu, this means 'sudo apt remove libboost-all-dev'), then we follow
            # the instructions at https://www.boost.org/more/getting_started/index.html.
            #
            # It's best to leave the default install location: headers in the 'boost' subdirectory of
            # /usr/local/include and libraries in /usr/local/lib.
            #
            # (It might initially _seem_ a good idea to put things in the same place as the distro packages, ie
            # running './bootstrap.sh --prefix=/usr' to put headers in /usr/include and libraries in /usr/lib.
            # However, this will mean that Meson cannot find the manually-installed Boost, even though it can find
            # distro-installed Boost in this location.)  So, eg, for Boost 1.80 on Linux, this means the following
            # in the shell:
            #
            #    cd ~
            #    mkdir ~/boost-tmp
            #    cd ~/boost-tmp
            #    wget https://boostorg.jfrog.io/artifactory/main/release/1.80.0/source/boost_1_80_0.tar.bz2
            #    tar --bzip2 -xf boost_1_80_0.tar.bz2
            #    cd boost_1_80_0
            #    ./bootstrap.sh
            #    sudo ./b2 install
            #    cd ~
            #    sudo rm -rf ~/boost-tmp
            #
            # We can handle the temporary directory stuff more elegantly (ie RAII style) in Python however
            #
            # NOTE: On older versions of Linux, there are problems building some of the Boost libraries that I haven't
            #       got to the bottom of.  Since, for now, we only use the following Boost libraries, we use additional
            #       options on the b2 command to limit what it builds:
            #          algorithm
            #          json
            #          stacktrace
            #       The list above can be recreated by running the following in the mbuild directory:
            #          grep -r '#include <boost' ../src | grep -i boost | sed 's+^.*#include <boost/++; s+/.*$++; s+.hpp.*$++' | sort -u
            #
            with tempfile.TemporaryDirectory(ignore_cleanup_errors = True) as tmpDirName:
               previousWorkingDirectory = pathlib.Path.cwd().as_posix()
               os.chdir(tmpDirName)
               log.debug('Working directory now ' + pathlib.Path.cwd().as_posix())
               boostUnderscoreName = 'boost_' + str(boostVersionToInstall).replace('.', '_')
               downloadFile(
                  'https://boostorg.jfrog.io/artifactory/main/release/' + str(boostVersionToInstall) + '/source/' +
                  boostUnderscoreName + '.tar.bz2'
               )
               log.debug('Boost download completed')
               shutil.unpack_archive(boostUnderscoreName + '.tar.bz2')
               log.debug('Boost archive extracted')
               os.chdir(boostUnderscoreName)
               log.debug('Working directory now ' + pathlib.Path.cwd().as_posix())
               btUtils.abortOnRunFail(subprocess.run(['./bootstrap.sh', '--with-python=python3']))
               log.debug('Boost bootstrap finished')
               btUtils.abortOnRunFail(subprocess.run(
                  ['sudo', './b2', '--with-algorithm',
                                   '--with-json',
                                   '--with-stacktrace',
                                   'install'])
               )
               log.debug('Boost install finished')
               os.chdir(previousWorkingDirectory)
               log.debug('Working directory now ' + pathlib.Path.cwd().as_posix() + '.  Removing ' + tmpDirName)
               #
               # The only issue with the RAII approach to removing the temporary directory is that some of the files
               # inside it will be owned by root, so there will be a permissions error when Python attempts to delete
               # the directory tree.  Fixing the permissions beforehand is a slightly clunky way around this.
               #
               btUtils.abortOnRunFail(
                  subprocess.run(
                     ['sudo', 'chmod', '--recursive', 'a+rw', tmpDirName]
                  )
               )

         #
         # Ubuntu 20.04 packages only have Meson 0.53.2, and we need 0.60.0 or later.  In this case it means we have to
         # install Meson via pip, which is not ideal on Linux.
         #
         # Specifically, as explained at https://mesonbuild.com/Getting-meson.html#installing-meson-with-pip, although
         # using the pip3 install gets a newer version, we have to do the pip install as root (which is normally not
         # recommended).  If we don't do this, then running `meson install` (or even `sudo meson install`) will barf on
         # Linux (because we need to be able to install files into system directories).
         #
         # So, where a sufficiently recent version of Meson is available in the distro packages (eg
         # `sudo apt install meson` on Ubuntu etc) it is much better to install this.   Installing via pip is a last
         # resort.
         #
         # The distro ID we get from 'lsb_release -is' will be 'Ubuntu' for all the variants of Ubuntu (eg including
         # Kubuntu).  Not sure what happens on derivatives such as Linux Mint though.
         #
         distroName = str(
            btUtils.abortOnRunFail(subprocess.run(['lsb_release', '-is'], encoding = "utf-8", capture_output = True)).stdout
         ).rstrip()
         log.debug('Linux distro: ' + distroName)
         if ('Ubuntu' == distroName):
            ubuntuRelease = str(
               btUtils.abortOnRunFail(subprocess.run(['lsb_release', '-rs'], encoding = "utf-8", capture_output = True)).stdout
            ).rstrip()
            log.debug('Ubuntu release: ' + ubuntuRelease)
            if (Decimal(ubuntuRelease) < Decimal('22.04')):
               log.info('Installing newer version of Meson the hard way')
               btUtils.abortOnRunFail(subprocess.run(['sudo', 'apt', 'remove', '-y', 'meson']))
               btUtils.abortOnRunFail(subprocess.run(['sudo', 'pip3', 'install', 'meson']))

      #-----------------------------------------------------------------------------------------------------------------
      #--------------------------------------------- Windows Dependencies ----------------------------------------------
      #-----------------------------------------------------------------------------------------------------------------
      case 'Windows':
         log.debug('Windows')
         #
         # First thing is to detect whether we're in the MSYS2 environment, and, if so, whether we're in the right
         # version of it.
         #
         # We take the existence of an executable `uname` in the path as a pretty good indicator that we're in MSYS2
         # or similar environment).  Then the result of running that should tell us if we're in the 32-bit version of
         # MSYS2.  (See comment below on why we don't yet support the 64-bit version, though I'm sure we'll fix this one
         # day.)
         #
         exe_uname = shutil.which('uname')
         if (exe_uname is None or exe_uname == ''):
            log.critical('Cannot find uname.  This script needs to be run under MSYS2 - see https://www.msys2.org/')
            exit(1)
         # We could just run uname without the -a option, but the latter gives some useful diagnostics to log
         unameResult = str(
            btUtils.abortOnRunFail(subprocess.run([exe_uname, '-a'], encoding = "utf-8", capture_output = True)).stdout
         ).rstrip()
         log.debug('Running uname -a gives ' + unameResult)
         # Output from `uname -a` will be of the form
         #    MINGW64_NT-10.0-19044 Matt-Virt-Win 3.4.3.x86_64 2023-01-11 20:20 UTC x86_64 Msys
         # We just need the bit before the first underscore, eg
         #    MINGW64
         terminalVersion = unameResult.split(sep='_', maxsplit=1)[0]


         if (terminalVersion != 'MINGW64'):
            # In the past, we built only 32-bit packages (i686 architecture) on Windows because of problems getting
            # 64-bit versions of NSIS plugins to work.  However, we now invoke NSIS without plugins, so the 64-bit build
            # seems to be working.
            #
            # As of January 2024, some of the 32-bit MSYS2 packages/groups we were previously relying on previously are
            # no longer available.  So now, we only build 64-bit packages (x86_64 architecture) on Windows.
            log.critical('Running in ' + terminalVersion + ' but need to run in MINGW64 (ie 64-bit build environment)')
            exit(1)

         # Ensure pip is up-to-date.  This is what the error message tells you to run if it's not!
         log.info('Ensuring Python pip is up-to-date')
         btUtils.abortOnRunFail(subprocess.run(['python3.exe', '-m', 'pip', 'install', '--upgrade', 'pip']))

         #
         # When we update packages below, we get "error: failed to commit transaction (conflicting files)" errors for a
         # bunch of Python packaging files unless we force Pacman to overwrite them.  This is somewhat less of a hack
         # than specifying --overwrite globally.
         #
         # We have to do both 32-bit and 64-bit versions of Python here to be certain.
         #
         # Note that the rules for glob expansion are different when invoking a command from Python then when typing it
         # on the command line, hence why the overwrite parameter is '*python*' not '"*python*"'.
         #
         log.info('Ensuring Python packaging is up-to-date')
         btUtils.abortOnRunFail(subprocess.run(['pacman', '-S', '--noconfirm', '--overwrite', '*python*', 'mingw-w64-i686-python-packaging']))
         btUtils.abortOnRunFail(subprocess.run(['pacman', '-S', '--noconfirm', '--overwrite', '*python*', 'mingw-w64-x86_64-python-packaging']))

         #
         # Before we install packages, we want to make sure the MSYS2 installation itself is up-to-date, otherwise you
         # can hit problems
         #
         #   pacman -S -y should download a fresh copy of the master package database
         #   pacman -S -u should upgrades all currently-installed packages that are out-of-date
         #
         log.info('Ensuring required libraries and frameworks are installed')
         btUtils.abortOnRunFail(subprocess.run(['pacman', '-S', '-y', '--noconfirm']))
         btUtils.abortOnRunFail(subprocess.run(['pacman', '-S', '-u', '--noconfirm']))

         #
         # We _could_ just invoke pacman once with the list of everything we want to install.  However, this can make
         # debugging a bit harder when there is a pacman problem, because it doesn't always give the most explanatory
         # error messages.  So we loop round and install one thing at a time.
         #
         # Note that the --disable-download-timeout option on Pacman proved often necessary because of slow mirror
         # sites, so we now specify it routinely.
         #
         # As noted above, we no longer support 32-bit ('i686') builds and now only support 64-bit ('x86_64') ones.
         #
         arch = 'x86_64'
         installList = ['base-devel',
                        'cmake',
                        'coreutils',
                        'doxygen',
                        'gcc',
                        'git',
                        'mingw-w64-' + arch + '-boost',
                        'mingw-w64-' + arch + '-cmake',
                        'mingw-w64-' + arch + '-libbacktrace',
                        'mingw-w64-' + arch + '-meson',
                        'mingw-w64-' + arch + '-nsis',
                        'mingw-w64-' + arch + '-freetype', #
                        'mingw-w64-' + arch + '-harfbuzz', #
                        'mingw-w64-' + arch + '-qt5-base',
                        'mingw-w64-' + arch + '-qt5-static',
                        'mingw-w64-' + arch + '-qt5',
                        'mingw-w64-' + arch + '-toolchain',
                        'mingw-w64-' + arch + '-xalan-c',
                        'mingw-w64-' + arch + '-xerces-c']
         for packageToInstall in installList:
            log.debug('Installing ' + packageToInstall)
            btUtils.abortOnRunFail(
               subprocess.run(
                  ['pacman', '-S', '--needed', '--noconfirm', '--disable-download-timeout', packageToInstall]
               )
            )

         #
         # Download NSIS plugins
         #
         # In theory we can use RAII here, eg:
         #
         #   with tempfile.TemporaryDirectory(ignore_cleanup_errors = True) as tmpDirName:
         #      previousWorkingDirectory = pathlib.Path.cwd().as_posix()
         #      os.chdir(tmpDirName)
         #      ...
         #      os.chdir(previousWorkingDirectory)
         #
         # However, in practice, this gets messy when there is an error (eg download fails) because Windows doesn't like
         # deleting files or directories that are in use.  So, in the event of the script needing to terminate early,
         # you get loads of errors, up to and including "maximum recursion depth exceeded" which rather mask whatever
         # the original problem was.
         #
         tmpDirName = tempfile.mkdtemp()
         previousWorkingDirectory = pathlib.Path.cwd().as_posix()
         os.chdir(tmpDirName)
         downloadFile('https://nsis.sourceforge.io/mediawiki/images/a/af/Locate.zip')
         shutil.unpack_archive('Locate.zip', 'Locate')
         downloadFile('https://nsis.sourceforge.io/mediawiki/images/7/76/Nsislog.zip')
         shutil.unpack_archive('Nsislog.zip', 'Nsislog')
         copyFilesToDir(['Locate/Include/Locate.nsh'], '/mingw32/share/nsis/Include/')
         copyFilesToDir(['Locate/Plugin/locate.dll',
                         'Nsislog/plugin/nsislog.dll'],'/mingw32/share/nsis/Plugins/ansi/')
         os.chdir(previousWorkingDirectory)
         shutil.rmtree(tmpDirName, ignore_errors=False)

      #-----------------------------------------------------------------------------------------------------------------
      #---------------------------------------------- Mac OS Dependencies ----------------------------------------------
      #-----------------------------------------------------------------------------------------------------------------
      case 'Darwin':
         log.debug('Mac')
         #
         # We install most of the Mac dependencies via Homebrew (https://brew.sh/) using the `brew` command below.
         # However, as at 2023-12-01, Homebrew has stopped supplying a package for Xalan-C.  So, we install that using
         # MacPorts (https://ports.macports.org/), which provides the `port` command.
         #
         # Note that MacPorts (port) requires sudo but Homebrew (brew) does not.   Perhaps more importantly, they two
         # package managers install things to different locations:
         #  - Homebrew packages are installed under /usr/local/Cellar/ with symlinks in /usr/local/opt/
         #  - MacPorts packages are installed under /opt/local
         # This means we need to have both directories in the include path when we come to compile.  Thankfully, both
         # CMake and Meson take care of finding a library automatically once given its name.
         #
         # Note too that package names vary slightly between HomeBrew and MacPorts.  Don't assume you can guess one from
         # the other, as it's not always evident.
         #

         #
         # Installing Homebrew is, in theory, somewhat easier and more self-contained than MacPorts as you just run the
         # following:
         #    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
         # In practice, invoking that command from this script is a bit fiddly to get right.  For the moment, we simply
         # assume Homebrew is already installed (because it is on the GitHub actions).
         #

         #
         # We install as many of our dependencies as possible with with Homebrew, and do this first, because some of
         # these packages will also be needed for the installation of MacPorts to work.
         #
         # We could make this list shorter if we wanted as, eg, installing Xalan-C will cause Xerces-C to be installed
         # too (as the former depends on the latter).  However, I think it's clearer to explicitly list all the direct
         # dependencies (eg we do make calls directly into Xerces).
         #
         # For the moment, we install Qt 5 (= 5.15.13), as there are code changes required to use Qt 6
         #
         # .:TBD:. Installing Boost here doesn't seem to give us libboost_stacktrace_backtrace
         #         Also, trying to use the "--cc=clang" option to install boost gives an error ("Error: boost: no bottle
         #         available!")  For the moment, we're just using Boost header files on Mac though, so this should be
         #         OK.
         #
         # We install the tree command here as, although it's not needed to do the build itself, it's useful for
         # diagnosing certain build problems (eg to see what changes certain parts of the build have made to the build
         # directory tree) when the build is running as a GitHub action.
         #
         installListBrew = ['llvm',
                            'gcc',
                            'cmake',
                            'coreutils',
                            'boost',
                            'doxygen',
                            'git',
                            'meson',
                            'ninja',
                            'pandoc',
                            'tree',
                            'qt@5',
#                            'xalan-c',
#                            'xerces-c'
                            ]
         for packageToInstall in installListBrew:
            log.debug('Installing ' + packageToInstall + ' via Homebrew')
            btUtils.abortOnRunFail(subprocess.run(['brew', 'install', packageToInstall]))
         #
         # By default, even once Qt5 is installed, Meson will not find it
         #
         # See https://stackoverflow.com/questions/29431882/get-qt5-up-and-running-on-a-new-mac for suggestion to do
         # the following to run `brew link qt5 --force` to "symlink the various Qt5 binaries and libraries into your
         # /usr/local/bin and /usr/local/lib directories".
         #
         btUtils.abortOnRunFail(subprocess.run(['brew', 'link', '--force', 'qt5']))

         #
         # Additionally, per lengthy discussion at https://github.com/Homebrew/legacy-homebrew/issues/29938, it seems
         # we might also need either:
         #    ln -s /usr/local/Cellar/qt5/5.15.7/mkspecs /usr/local/mkspecs
         #    ln -s /usr/local/Cellar/qt5/5.15.7/plugins /usr/local/plugins
         # or:
         #    export PATH=/usr/local/opt/qt5/bin:$PATH
         # The former gives permission errors, so we do the latter in mac.yml
         #
         # But the brew command to install Qt also tells us to do the following:
         #
         #    echo 'export PATH="/usr/local/opt/qt@5/bin:$PATH"' >> ~/.bash_profile
         #    export LDFLAGS="-L/usr/local/opt/qt@5/lib"
         #    export CPPFLAGS="-I/usr/local/opt/qt@5/include"
         #    export PKG_CONFIG_PATH="/usr/local/opt/qt@5/lib/pkgconfig"
         #
         # Note however that, in a GitHub runner, the first of these will give "[Errno 13] Permission denied".
         #
         try:
            # See
            # https://stackoverflow.com/questions/1466000/difference-between-modes-a-a-w-w-and-r-in-built-in-open-function
            # for a good summary (clearer than the Python official docs) of the mode flag on open.
            with open("~/.bash_profile", "a+") as bashProfile:
               bashProfile.write('export PATH="/usr/local/opt/qt@5/bin:$PATH"')
         except IOError as ioe:
            # This is not fatal, so we just note the error and continue
            log.warning("Unable to write to .bash_profile: " + ioe.strerror)
         os.environ['LDFLAGS'] = '-L/usr/local/opt/qt@5/lib'
         os.environ['CPPFLAGS'] = '-I/usr/local/opt/qt@5/include'
         os.environ['PKG_CONFIG_PATH'] = '/usr/local/opt/qt@5/lib/pkgconfig'

         #
         # See comment about CMAKE_PREFIX_PATH in CMakeLists.txt.  I think this is rather too soon to try to do this,
         # but, it can't hurt.
         #
         # Typically, this is going to set CMAKE_PREFIX_PATH to /usr/local/opt/qt@5
         #
         qtPrefixPath = btUtils.abortOnRunFail(
            subprocess.run(['brew', '--prefix', 'qt5'], capture_output=True)
         ).stdout.decode('UTF-8').rstrip()
         log.debug('Qt Prefix Path: ' + qtPrefixPath)
         os.environ['CMAKE_PREFIX_PATH'] = qtPrefixPath;
         btUtils.abortOnRunFail(subprocess.run(['tree', '-sh', qtPrefixPath], capture_output=False))

         #
         # In theory, we can now install MacPorts.  However, I didn't manage to get the following working.  The
         # configure script complains about the lack of /usr/local/.//mkspecs/macx-clang.  So, for now, we comment this
         # bit out and install MacPorts for GitHub actions via the mac.yml script.
         #
         # This is a source install - per instructions at https://guide.macports.org/#installing.macports.source.  In
         # principle, we could install a precompiled binary, but it's a bit painful to do programatically as even to
         # download the right package you have to know not just the the Darwin version of MacOS you are running, but
         # also its "release name" (aka "friendly name").  See
         # https://apple.stackexchange.com/questions/333452/how-can-i-find-the-friendly-name-of-the-operating-system-from-the-shell-term
         # for various ways to do this if we had to, though we might just as easily copy the info
         # from https://en.wikipedia.org/wiki/MacOS#Timeline_of_releases
         #
##         log.debug('Installing MacPorts')
##         btUtils.abortOnRunFail(subprocess.run(['curl', '-O', 'https://distfiles.macports.org/MacPorts/MacPorts-2.8.1.tar.bz2']))
##         btUtils.abortOnRunFail(subprocess.run(['tar', 'xf', 'MacPorts-2.8.1.tar.bz2']))
##         btUtils.abortOnRunFail(subprocess.run(['cd', 'MacPorts-2.8.1/']))
##         btUtils.abortOnRunFail(subprocess.run(['./configure']))
##         btUtils.abortOnRunFail(subprocess.run(['make']))
##         btUtils.abortOnRunFail(subprocess.run(['sudo', 'make', 'install']))
##         btUtils.abortOnRunFail(subprocess.run(['export', 'PATH=/opt/local/bin:/opt/local/sbin:$PATH']))

         #
         # Now install Xalan-C via MacPorts
         #
         installListPort = ['xalanc',
                            'xercesc3']
         for packageToInstall in installListPort:
            log.debug('Installing ' + packageToInstall + ' via MacPorts')
            btUtils.abortOnRunFail(subprocess.run(['sudo', 'port', 'install', packageToInstall]))

         #
         # dmgbuild is a Python package that provides a command line tool to create macOS disk images (aka .dmg
         # files) -- see https://dmgbuild.readthedocs.io/en/latest/
         #
         # Note that we install with the [badge_icons] extra so we can use the badge_icon setting (see
         # https://dmgbuild.readthedocs.io/en/latest/settings.html#badge_icon)
         #
         btUtils.abortOnRunFail(subprocess.run(['pip3', 'install', 'dmgbuild[badge_icons]']))

      case _:
         log.critical('Unrecognised platform: ' + platform.system())
         exit(1)

   #--------------------------------------------------------------------------------------------------------------------
   #------------------------------------------- Cross-platform Dependencies --------------------------------------------
   #--------------------------------------------------------------------------------------------------------------------
   #
   # We use libbacktrace from https://github.com/ianlancetaylor/libbacktrace.  It's not available as a Debian package
   # and not any more included with GCC by default.  It's not a large library so, unless and until we start using Conan,
   # the easiest approach seems to be to bring it in as a Git submodule and compile from source.
   #
   ensureSubmodulesPresent()
   log.info('Checking libbacktrace is built')
   previousWorkingDirectory = pathlib.Path.cwd().as_posix()
   backtraceDir = dir_gitSubmodules.joinpath('libbacktrace')
   os.chdir(backtraceDir)
   log.debug('Run configure and make in ' + backtraceDir.as_posix())
   #
   # We only want to configure and compile libbacktrace once, so we do it here rather than in Meson.build
   #
   # Libbacktrace uses autoconf/automake so it's relatively simple to build, but for a couple of gotchas
   #
   # Note that, although on Linux you can just invoke `./configure`, this doesn't work in the MSYS2 environment, so,
   # knowing that 'configure' is a shell script, we invoke it as such.  However, we must be careful to run it with the
   # correct shell, specifically `sh` (aka dash on Linux) rather than `bash`.  Otherwise, the Makefile it generates will
   # not work properly, and we'll end up building a library with missing symbols that gives link errors on our own
   # executables.
   #
   # (I haven't delved deeply into this but, confusingly, if you run `sh ./configure` it puts 'SHELL = /bin/bash' in the
   # Makefile, whereas, if you run `bash ./configure`, it puts the line 'SHELL = /bin/sh' in the Makefile.)
   #
   btUtils.abortOnRunFail(subprocess.run(['sh', './configure']))
   btUtils.abortOnRunFail(subprocess.run(['make']))
   os.chdir(previousWorkingDirectory)

   log.info('*** Finished checking / installing dependencies ***')
   return

#-----------------------------------------------------------------------------------------------------------------------
# ./bt setup
#-----------------------------------------------------------------------------------------------------------------------
def doSetup(setupOption):
   if (setupOption == 'all'):
      installDependencies()

   findMesonAndGit()

   # If this is a git checkout then let's set up git with the project standards
   if (dir_gitInfo.is_dir()):
      log.info('Setting up ' + capitalisedProjectName + ' git preferences')
      # Enforce indentation with spaces, not tabs.
      btUtils.abortOnRunFail(
         subprocess.run(
            [exe_git,
               "config",
               "--file", dir_gitInfo.joinpath('config').as_posix(),
               "core.whitespace",
               "tabwidth=3,tab-in-indent"],
            capture_output=False
         )
      )

      # Enable the standard pre-commit hook that warns you about whitespace errors
      shutil.copy2(dir_gitInfo.joinpath('hooks/pre-commit.sample'),
                   dir_gitInfo.joinpath('hooks/pre-commit'))

      ensureSubmodulesPresent()

   # Check whether Meson build directory is already set up.  (Although nothing bad happens, if you run setup twice,
   # it complains and tells you to run configure.)
   # Best clue that set-up has been run (rather than, say, user just created empty mbuild directory by hand) is the
   # presence of meson-info/meson-info.json (which is created by setup for IDE integration -- see
   # https://mesonbuild.com/IDE-integration.html#ide-integration)
   runMesonSetup = True
   warnAboutCurrentDirectory = False
   if (dir_build.joinpath('meson-info/meson-info.json').is_file()):
      log.info('Meson build directory ' + dir_build.as_posix() + ' appears to be already set up')
      #
      # You usually only need to reset things after you've done certain edits to defaults etc in meson.build.  There
      # are a whole bunch of things you can control with the 'meson configure' command, but it's simplest in some ways
      # just to reset the build directory and rely on meson setup picking up defaults from meson.build.
      #
      # Note that we don't have to worry about this prompt appearing in a GitHub action, because we are always creating
      # the mbuild directory for the first time when this script is run in such actions -- ie we should never reach this
      # part of the code.
      #
      response = ""
      while (response != 'y' and response != 'n'):
         response = input('Do you want to completely reset the build directory? [y or n] ').lower()
      if (response == 'n'):
         runMesonSetup = False
      else:
         # It's very possible that the user's current working directory is mbuild.  If so, we need to warn them and move
         # up a directory (as 'meson setup' gets upset if current working directory does not exist).
         log.info('Removing existing Meson build directory ' + dir_build.as_posix())
         if (pathlib.Path.cwd().as_posix() == dir_build.as_posix()):
            # We write a warning out here for completeness, but we really need to show it further down as it will have
            # scrolled off the top of the terminal with all the output from 'meson setup'
            log.warning('You are currently in the directory we are about to delete.  ' +
                        'You will need to change directory!')
            warnAboutCurrentDirectory = True
            os.chdir(dir_base)
         shutil.rmtree(dir_build)

   if (runMesonSetup):
      log.info('Setting up ' + dir_build.as_posix() + ' meson build directory')
      # See https://mesonbuild.com/Commands.html#setup for all the optional parameters to meson setup
      # Note that meson setup will create the build directory (along with various subdirectories)
      btUtils.abortOnRunFail(subprocess.run([exe_meson, "setup", dir_build.as_posix(), dir_base.as_posix()],
                                    capture_output=False))

      log.info('Finished setting up Meson build.  Note that the warnings above about path separator and optimization ' +
               'level are expected!')

   if (warnAboutCurrentDirectory):
      print("❗❗❗ Your current directory has been deleted!  You need to run 'cd ../mbuild' ❗❗❗")
   log.debug('Setup done')
   print()
   print('You can now build, test, install and run ' + capitalisedProjectName + ' with the following commands:')
   print('   cd ' + os.path.relpath(dir_build))
   print('   meson compile')
   print('   meson test')
   if (platform.system() == 'Linux'):
      print('   sudo meson install')
   else:
      print('   meson install')
   print('   ' + projectName)


   return

#-----------------------------------------------------------------------------------------------------------------------
# ./bt package
#-----------------------------------------------------------------------------------------------------------------------
def doPackage():
   #
   # Meson does not offer a huge amount of help on creating installable packages.  It has no equivalent to CMake's CPack
   # and there is generally not a lot of info out there about how to do packaging in Meson.  In fact, it seems unlikely
   # that packaging will ever come within it scope.  (Movement is rather in the other direction - eg there _used_ to be
   # a Meson module for creating RPMs, but it was discontinued per
   # https://mesonbuild.com/Release-notes-for-0-62-0.html#removal-of-the-rpm-module because it was broken and badly
   # designed etc.)
   #
   # At first, this seemed disappointing, but I've rather come around to thinking a different way about it.  Although
   # CPack has lots of features, it is also very painful to use.  Some of the things you can do are undocumented; some
   # of the things you want to be able to do seem nigh on impossible.  So perhaps taking a completely different
   # approach, eg using a scripting language rather than a build tool to do packaging, is ultimately a good thing.
   #
   # I spent some time looking at and trying to use the Qt-Installer-Framework (QtIFW).  Upsides are:
   #   - In principle we could write one set of install config that would then create install packages for Windows, Mac
   #     and Linux.
   #   - It should already know how to package Qt libraries(!)
   #   - It's the same licence as the rest of Qt.
   #   - We could use it in GitHub actions (courtesy of https://github.com/jurplel/install-qt-action).
   #   - It can handle in-place upgrades (including the check for whether an upgraded version is available), per
   #     https://doc.qt.io/qtinstallerframework/ifw-updates.html.
   # Downsides are:
   #   - Outside of packaging Qt itself, I'm not sure that it's hugely widely used.  It can be hard to find "how tos" or
   #     other assistance.
   #   - It's not a great advert for itself -- eg when I installed it locally on Kubuntu by downloading directly from
   #     https://download.qt.io/official_releases/qt-installer-framework/, it didn't put its own tools in the PATH,
   #     so I had to manually add ~/Qt/QtIFW-4.5.0/bin/ to my PATH.
   #   - It usually necessary to link against a static build of Qt, which is a bit of a pain as you have to download the
   #     source files for Qt and compile it locally -- see eg
   #     https://stackoverflow.com/questions/14932315/how-to-compile-qt-5-under-windows-or-linux-32-or-64-bit-static-or-dynamic-on-v
   #     for the whole process.
   #   - It's a change of installation method for people who have previously downloaded deb packages, RPMs, Mac DMG
   #     files, etc.
   #   - It puts things in different places than 'native' installers.  Eg, on Linux, everything gets installed to
   #     subdirectories of the user's home directory rather than the "usual" system directories).  Amongst other things,
   #     this makes it harder for distros etc that want to ship our software as "standard" packages.
   #
   # The alternative approach, which I resisted for a fair while, but have ultimately become persuaded is right, is to
   # do Windows, Mac and Linux packaging separately:
   #   - For Mac, there is some info at https://mesonbuild.com/Creating-OSX-packages.html on creating app bundles
   #   - For Linux, there is some mention in the Meson manual of building deb and rpm packages eg
   #     https://mesonbuild.com/Installing.html#destdir-support, but I think you have to do most of the work yourself.
   #     https://blog.devgenius.io/how-to-build-debian-packages-from-meson-ninja-d1c28b60e709 gives some sketchy
   #     starting info on how to build deb packages.  Maybe we could find the equivalent for creating RPMs.  Also look
   #     at https://openbuildservice.org/.
   #   - For Windows, we use NSIS (Nullsoft Scriptable Install System -- see https://nsis.sourceforge.io/) -- to create
   #     a Windows installer.
   #
   # Although a lot of packaging is platform-specific, the initial set-up is generic.
   #
   #    1. This script (as invoked directly) creates some packaging sub-directories of the build directory and then
   #       invokes Meson
   #    2. Meson installs all the binaries, data files and so on that we need to ship into the packaging directory tree
   #    3. Meson also exports a bunch of build information into a TOML file that we read in.  This saves us duplicating
   #       too many meseon.build settings in this file.
   #

   findMesonAndGit()

   #
   # The top-level directory structure we create inside the build directory (mbuild) for packaging is:
   #
   #    packages/   Holds the subdirectories below, plus the source tarball and its checksum
   #    │
   #    ├── windows/ For Windows
   #    │
   #    ├── darwin/  For Mac
   #    │
   #    ├── linux/   For Linux
   #    │
   #    └── source/   For source code tarball
   #
   # NB: If we wanted to change this, we would need to make corresponding changes in meson.build
   #

   # Step 1 : Create a top-level package directory structure
   #          We'll make the relevant top-level directory and ensure it starts out empty
   #          (We don't have to make dir_packages as it will automatically get created by os.makedirs when we ask it to
   #          create dir_packages_platform.)
   if (dir_packages_platform.is_dir()):
      log.info('Removing existing ' + dir_packages_platform.as_posix() + ' directory tree')
      shutil.rmtree(dir_packages_platform)
   log.info('Creating directory ' + dir_packages_platform.as_posix())
   os.makedirs(dir_packages_platform)

   # We change into the build directory.  This doesn't affect the caller (of this script) because we're a separate
   # sub-process from the (typically) shell that invoked us and we cannot change the parent process's working
   # directory.
   os.chdir(dir_build)
   log.debug('Working directory now ' + pathlib.Path.cwd().as_posix())

   #
   # Meson can't do binary packaging, but it can create a source tarball for us via `meson dist`.  We use the following
   # options:
   #    --no-tests  = stops Meson doing a full build and test, on the assumption that we've already done this by the
   #                  time we come to packaging
   #    --allow-dirty  = allow uncommitted changes, which is needed in Meson 0.62 and later to prevent Meson emitting a
   #                     fatal error if there are uncommitted changes on the current git branch.  (In previous versions
   #                     of Meson, this was just a warning.)  NOTE that, even with this option specified, uncommitted
   #                     changes will be ignored (ie excluded from the source tarball).
   #
   # Of course, we could create a compressed tarball directly in this script, but the advantage of having Meson do it is
   # that it will (I believe) include only source & data files actually in the git repository in meson.build, so you
   # won't pick up other things that happen to be hanging around in the source etc directory trees.
   #
   log.info('Creating source tarball')
   if (mesonVersion >= packaging.version.parse('0.62.0')):
      btUtils.abortOnRunFail(
         subprocess.run([exe_meson, 'dist', '--no-tests', '--allow-dirty'], capture_output=False)
      )
   else:
      btUtils.abortOnRunFail(
         subprocess.run([exe_meson, 'dist', '--no-tests'], capture_output=False)
      )

   #
   # The source tarball and its checksum end up in the meson-dist subdirectory of mbuild, so we just move them into the
   # packages/source directory (first making sure the latter exists and is empty!).
   #
   # We are only talking about 2 files, so some of this is overkill, but it's easier to be consistent with what we do
   # for the other subdirectories of mbuild/packages
   #
   if (dir_packages_source.is_dir()):
      log.info('Removing existing ' + dir_packages_source.as_posix() + ' directory tree')
      shutil.rmtree(dir_packages_source)
   log.info('Creating directory ' + dir_packages_source.as_posix())
   os.makedirs(dir_packages_source)
   meson_dist_dir = dir_build.joinpath('meson-dist')
   for fileName in os.listdir(meson_dist_dir.as_posix()):
      log.debug('Moving ' + fileName + ' from ' + meson_dist_dir.as_posix() + ' to ' + dir_packages_source.as_posix())
      # shutil.move will error rather than overwrite an existing file, so we handle that case manually (although in
      # theory it should never arise)
      targetFile = dir_packages_source.joinpath(fileName)
      if os.path.exists(targetFile):
         log.debug('Removing old ' + targetFile)
         os.remove(targetFile)
      shutil.move(meson_dist_dir.joinpath(fileName), dir_packages_source)

   #
   # Running 'meson install' with the --destdir option will put all the installable files (program executable,
   # translation files, data files, etc) in subdirectories of the platform-specific packaging directory.  However, it
   # will not bundle up any shared libraries that we need to ship with the application on Windows and Mac.  We handle
   # this in the platform-specific code below.
   #
   log.info('Running meson install with --destdir option')
   # See https://mesonbuild.com/Commands.html#install for the optional parameters to meson install
   btUtils.abortOnRunFail(subprocess.run([exe_meson, 'install', '--destdir', dir_packages_platform.as_posix()],
                                 capture_output=False))

   #
   # At the direction of meson.build, Meson should have generated a config.toml file in the build directory that we can
   # read in to get useful settings exported from the build system.
   #
   global buildConfig
   with open(dir_build.joinpath('config.toml').as_posix()) as buildConfigFile:
      buildConfig = tomlkit.parse(buildConfigFile.read())
   log.debug('Shared libraries: ' + ', '.join(buildConfig["CONFIG_SHARED_LIBRARY_PATHS"]))

   #
   # Note however that there are some things that are (often intentionally) difficult or impossible to import to or
   # export from Meson.  (See
   # https://mesonbuild.com/FAQ.html#why-is-meson-not-just-a-python-module-so-i-could-code-my-build-setup-in-python for
   # why it an explicitly design goal not to have the Meson configuration language be Turing-complete.)
   #
   # We deal with some of these in platform-specific code below
   #

   #
   # If meson install worked, we can now do the actual packaging.
   #
   match platform.system():

      #-----------------------------------------------------------------------------------------------------------------
      #------------------------------------------------ Linux Packaging ------------------------------------------------
      #-----------------------------------------------------------------------------------------------------------------
      case 'Linux':
         #
         # There are, of course, multiple package managers in the Linux world.  We cater for two of the main ones,
         # Debian and RPM.
         #
         # Note, per https://en.wikipedia.org/wiki/X86-64, that x86_64 and amd64 are the same thing; the latter is just
         # a rebranding of the former by AMD.  Debian packages use 'amd64' in the filename, while RPM ones use 'x86_64',
         # but it's the same code being packaged and pretty much the same directory structure being installed into.
         #
         # Some of the processing we need to do is the same for Debian and RPM, so do that first before we copy things
         # into separate trees for actually building the packages
         #
         log.debug('Linux Packaging')

         #
         # First, note that Meson is geared up for building and installing locally.  (It doesn't really know about
         # packaging.)  This means it installs locally to /usr/local/bin, /usr/local/share, etc.  This is "correct" for
         # locally-built software but not for packaged software, which needs to go in /usr/bin, /usr/share, etc.  So,
         # inside the mbuild/packages directory tree, we just need to move everything out of linux/usr/local up one
         # level into linux/usr and then remove the empty linux/usr/local directory
         #
         log.debug('Moving usr/local files to usr inside ' + dir_packages_platform.as_posix())
         targetDir = dir_packages_platform.joinpath('usr')
         sourceDir = targetDir.joinpath('local')
         for fileName in os.listdir(sourceDir.as_posix()):
            shutil.move(sourceDir.joinpath(fileName), targetDir)
         os.rmdir(sourceDir.as_posix())

         #
         # Debian and RPM both want the debugging information stripped from the executable.
         #
         # .:TBD:. One day perhaps we could be friendly and still ship the debugging info, just in a separate .dbg
         # file.  The procedure to do this is described in the 'only-keep-debug' section of `man objcopy`.  However, we
         # need to work out where to put the .dbg file so that it remains usable but lintian does not complain about it.
         #
         dir_packages_bin = dir_packages_platform.joinpath('usr').joinpath('bin')
         log.debug('Stripping debug symbols')
         btUtils.abortOnRunFail(
            subprocess.run(
               ['strip',
                '--strip-unneeded',
                '--remove-section=.comment',
                '--remove-section=.note binaries',
                dir_packages_bin.joinpath(projectName)],
               capture_output=False
            )
         )

         #--------------------------------------------------------------------------------------------------------------
         #-------------------------------------------- Debian .deb Package ---------------------------------------------
         #--------------------------------------------------------------------------------------------------------------
         #
         # There are some relatively helpful notes on building debian packages at:
         #    https://unix.stackexchange.com/questions/30303/how-to-create-a-deb-file-manually
         #    https://www.internalpointers.com/post/build-binary-deb-package-practical-guide
         #
         # We skip a lot of things because we are not trying to ship a Debian source package, just a binary one.
         # (Debian wants source packages to be built with an old-fashioned makefile, which seems a bit too painful to
         # me.  Since there are other very easy routes for people to get the source code, I'm not rushing to jump
         # through a lot of hoops to package it up in a .deb file.)
         #
         # Skipping the source package means we don't (and indeed can't) use all the tools that come with dh-make and it
         # means we need to do a tiny bit more manual work in creating some parts of the install tree.  But, overall,
         # the process itself is simple once you've worked out what you need to do (which was slightly more painful than
         # you might have hoped).
         #
         # To create a deb package, we create the following directory structure, where items marked ✅ are copied as is
         # from the tree generated by meson install with --destdir option, and those marked ❇ are ones we need to
         # relocate, generate or modify.
         #
         # (When working on this bit, use ❌ for things that are generated automatically but not actually needed, and ✴
         # for things we still need to add.  Not currently not aware of any of either.)
         #    debbuild
         #    └── [projectName]-[versionNumber]-1_amd64
         #        ├── DEBIAN
         #        │   └── control          ❇  # Contains info about dependencies, maintainer, etc
         #        │
         #        └── usr
         #            ├── bin
         #            │   └── [projectName] ✅   <── the executable
         #            └── share
         #                ├── applications
         #                │   └── [projectName].desktop     ✅  <── [filesToInstall_desktop]
         #                ├── [projectName]
         #                │   ├── DefaultData.xml           ✅  <──┬── [filesToInstall_data]
         #                │   ├── default_db.sqlite         ✅  <──┘
         #                │   ├── sounds
         #                │   │   └── [All the filesToInstall_sounds .wav files] ✅
         #                │   └── translations_qm
         #                │       └── [All the .qm files generated by qt.compile_translations] ✅
         #                ├── doc
         #                │    └── [projectName]
         #                │        ├── changelog.Debian.gz            ✅
         #                │        ├── copyright                      ✅
         #                │        ├── README.md (or README.markdown) ✅
         #                │        └── RelaseNotes.markdown           ✅
         #                ├── icons
         #                │   └── hicolor
         #                │       └── scalable
         #                │           └── apps
         #                │               └── [projectName].svg ✅  <── [filesToInstall_icons]
         #                └── man
         #                    └── man1
         #                        └── [projectName].1.gz ❇ <── English version of man page (compressed)
         #

         # Make the top-level directory for the deb package and the DEBIAN subdirectory for the package control files
         # etc
         log.debug('Creating debian package top-level directories')
         debPackageDirName = projectName + '-' + buildConfig['CONFIG_VERSION_STRING'] + '-1_amd64'
         dir_packages_deb = dir_packages_platform.joinpath('debbuild').joinpath(debPackageDirName)
         dir_packages_deb_control = dir_packages_deb.joinpath('DEBIAN')
         os.makedirs(dir_packages_deb_control) # This will also automatically create parent directories
         dir_packages_deb_doc = dir_packages_deb.joinpath('usr/share/doc').joinpath(projectName)

         # Copy the linux/usr tree inside the top-level directory for the deb package
         log.debug('Copying deb package contents')
         shutil.copytree(dir_packages_platform.joinpath('usr'), dir_packages_deb.joinpath('usr'))

         #
         # Copy the Debian Binary package control file to where it belongs
         #
         log.debug('Copying deb package control file')
         copyWithoutCommentsOrFolds(dir_build.joinpath('control').as_posix(),
                                    dir_packages_deb_control.joinpath('control').as_posix())


         #
         # Generate compressed changelog for Debian package from markdown
         #
         # Each Debian package (which provides a /usr/share/doc/pkg directory) must install a Debian changelog file in
         # /usr/share/doc/pkg/changelog.Debian.gz
         #
         # This is done by a shell script because we already wrote that
         #
         log.debug('Generating compressed changelog')
         os.environ['CONFIG_APPLICATION_NAME_LC'    ] = buildConfig['CONFIG_APPLICATION_NAME_LC'    ]
         os.environ['CONFIG_CHANGE_LOG_UNCOMPRESSED'] = buildConfig['CONFIG_CHANGE_LOG_UNCOMPRESSED']
         os.environ['CONFIG_CHANGE_LOG_COMPRESSED'  ] = dir_packages_deb_doc.joinpath('changelog.Debian.gz').as_posix()
         os.environ['CONFIG_PACKAGE_MAINTAINER'     ] = buildConfig['CONFIG_PACKAGE_MAINTAINER'     ]
         btUtils.abortOnRunFail(
            subprocess.run([dir_base.joinpath('packaging').joinpath('generateCompressedChangeLog.sh')],
                           capture_output=False)
         )
         # Shell script gives wrong permissions on output (which lintian would complain about), so fix them here (from
         # rw-rw-r-- to rw-r--r--).
         os.chmod(dir_packages_deb_doc.joinpath('changelog.Debian.gz'),
                  stat.S_IRUSR | stat.S_IWUSR | stat.S_IRGRP | stat.S_IROTH)

         #
         # Debian packages want man pages to be compressed with gzip with the highest compression available (-9n).
         #
         # TBD: We'll need to expand this slightly when we support man pages in multiple languages.
         #
         # We _could_ do this all in Python with the gzip module, but it's somewhat less coding just to invoke the gzip
         # program directly
         #
         dir_packages_deb_man = dir_packages_deb.joinpath('usr').joinpath('share').joinpath('man')
         dir_packages_deb_man1 = dir_packages_deb_man.joinpath('man1')
         log.debug('Compressing man page')
         btUtils.abortOnRunFail(
            subprocess.run(['gzip', '-9n', dir_packages_deb_man1.joinpath(projectName + '.1')], capture_output=False)
         )

         #
         # Now we actually generate the package
         #
         # Generates the package with the same name as the package directory plus '.deb' on the end
         log.info('Generating deb package')
         previousWorkingDirectory = pathlib.Path.cwd().as_posix()
         os.chdir(dir_packages_platform.joinpath('debbuild'))
         btUtils.abortOnRunFail(
            subprocess.run(['dpkg-deb', '--build', '--root-owner-group', debPackageDirName], capture_output=False)
         )

         # The debian package name is (I think) derived from the name of the directory we supplied as build parameter
         debPackageName = debPackageDirName + '.deb'

         # Running lintian does a very strict check on the Debian package.  You can find a list of all the error and
         # warning codes at https://lintian.debian.org/tags.
         #
         # Some of the warnings are things that only matter for packages that actually ship with Debian itself - ie they
         # won't stop the package working but are not strictly within the standards that the Debian project sets for the
         # packages included in the distro.
         #
         # Still, we try to fix as many warnings as possible.  As at 2022-08-11 we currently have one warning that we do
         # not ship a man page.  We should get to this at some point.
         log.info('Running lintian to check the created deb package for errors and warnings')
         btUtils.abortOnRunFail(
            subprocess.run(['lintian', '--no-tag-display-limit', debPackageName], capture_output=False)
         )

         # Move the .deb file to the top-level directory
         shutil.move(debPackageName, dir_packages_platform)

         # We don't particularly need to change back to the previous working directory, but it's tidy to do so.
         os.chdir(previousWorkingDirectory)

         #
         # Make the checksum file
         #
         log.info('Generating checksum file for ' + debPackageName)
         writeSha256sum(dir_packages_platform, debPackageName)

         #--------------------------------------------------------------------------------------------------------------
         #---------------------------------------------- RPM .rpm Package ----------------------------------------------
         #--------------------------------------------------------------------------------------------------------------
         # This script is written assuming you are on a Debian-based Linux.
         #
         # In theory we can use `alien` to convert a .deb to a .rpm, but I worry that this would not handle dependencies
         # very well.  So we prefer to build a bit more manually.
         #
         # To create a rpm package, we create the following directory structure, where items marked ✅ are copied as is
         # from the tree generated by meson install with --destdir option, and those marked ❇ are ones we either
         # generate or modify.
         #
         # (When working on this bit, use ❌ for things that are generated automatically but not actually needed, and ✴
         # for things we still need to add.  Not currently not aware of any of either.)
         #    rpmbuild
         #    ├── SPECS
         #    │   └── rpm.spec ❇
         #    └── BUILDROOT
         #        └── usr
         #            ├── bin
         #            │   └── [projectName] ✅   <── the executable
         #            ├── lib
         #            │   └── .build-id
         #            └── share
         #                ├── applications
         #                │   └── [projectName].desktop     ✅  <── [filesToInstall_desktop]
         #                ├── [projectName]
         #                │   ├── DefaultData.xml           ✅  <──┬── [filesToInstall_data]
         #                │   ├── default_db.sqlite         ✅  <──┘
         #                │   ├── sounds
         #                │   │   └── [All the filesToInstall_sounds .wav files] ✅
         #                │   └── translations_qm
         #                │       └── [All the .qm files generated by qt.compile_translations] ✅
         #                ├── doc
         #                │    └── [projectName]
         #                │        ├── copyright                      ✅
         #                │        ├── README.md (or README.markdown) ✅
         #                │        └── RelaseNotes.markdown           ✅
         #                ├── icons
         #                │   └── hicolor
         #                │       └── scalable
         #                │           └── apps
         #                │               └── [projectName].svg ✅  <── [filesToInstall_icons]
         #                └── man
         #                    └── man1
         #                        └── [projectName].1.bz2 ❇ <── English version of man page (compressed)
         #
         #

         # Make the top-level directory for the rpm package and the SPECS subdirectory etc
         log.debug('Creating rpm package top-level directories')
         rpmPackageDirName = 'rpmbuild'
         dir_packages_rpm = dir_packages_platform.joinpath(rpmPackageDirName)
         dir_packages_rpm_specs = dir_packages_rpm.joinpath('SPECS')
         os.makedirs(dir_packages_rpm_specs) # This will also automatically create dir_packages_rpm
         dir_packages_rpm_buildroot = dir_packages_rpm.joinpath('BUILDROOT')
         os.makedirs(dir_packages_rpm_buildroot)

         # Copy the linux/usr tree inside the top-level directory for the rpm package
         log.debug('Copying rpm package contents')
         shutil.copytree(dir_packages_platform.joinpath('usr'), dir_packages_rpm_buildroot.joinpath('usr'))

         # Copy the RPM spec file, doing the same unfolding etc as for the Debian control file above
         log.debug('Copying rpm spec file')
         copyWithoutCommentsOrFolds(dir_build.joinpath('rpm.spec').as_posix(),
                                    dir_packages_rpm_specs.joinpath('rpm.spec').as_posix())

         #
         # In Debian packaging, the change log is a separate file.  However, for RPM packaging, the change log needs to
         # be, included in the spec file.  The simplest way to do that is for us to append it to the file we've just
         # copied.  (NB: This relies on the last line of that file being `%changelog` -- ie the macro that introduces
         # the change log.)
         #
         # Since we store our change log internally in markdown, we also convert it to the RPM format at the same time
         # as appending it.  (This is different from the Debian changelog format, so we can't just reuse what we've done
         # above.)  Per https://docs.fedoraproject.org/en-US/packaging-guidelines/#changelogs, the format we need is:
         #    %changelog
         #    * Wed Jun 14 2003 Joe Packager <joe at gmail.com> - 1.0-2
         #    - Added README file (#42).
         # (Note that we don't have to write '%changelog' as it's already in the spec file.)
         # The format we have is:
         #    ## v3.0.2
         #    Minor bug fixes for the 3.0.1 release (ie bugs in 3.0.1 are fixed in this 3.0.2 release).
         #
         #    ### New Features
         #
         #    * None
         #
         #    ### Bug Fixes
         #    * LGPL-2.1-only and LGPL-3.0-only license text not shipped [#664](https://github.com/Brewtarget/brewtarget/issues/664)
         #    * Release 3.0.1 is uninstallable on Ubuntu 22.04.1 [#665](https://github.com/Brewtarget/brewtarget/issues/665)
         #    * Turkish Language selection in settings not working [#670])https://github.com/Brewtarget/brewtarget/issues/670)
         #
         #    ### Release Timestamp
         #    Wed, 26 Oct 2022 10:10:10 +0100
         #
         #    ## v3.0.1
         #    etc
         #
         with open(os.environ['CONFIG_CHANGE_LOG_UNCOMPRESSED'], 'r') as markdownChangeLog, open(dir_packages_rpm_specs.joinpath('rpm.spec'), 'a') as specFile:
            inIntro = True
            releaseDate = ''
            versionNumber = ''
            changes = []
            for line in markdownChangeLog:
               if (inIntro):
                  # Skip over the introductory headings and paragraphs of CHANGES.markdown until we get to the first
                  # version line, which begins with '## v'.
                  if (not line.startswith('## v')):
                     # Skip straight to processing the next line
                     continue
                  # We've reached the end of the introductory stuff, so the current line is the first one that we
                  # process "as normal" below.
                  inIntro = False
               # If this is a version line, it's the start of a change block (and the end of the previous one if there
               # was one).  Grab the version number (and write out the previous block if there was one).  Note that we
               # have to add the '-1' "package release" number on the end of the version number (but before the
               # newline!), otherwise rpmlint will complain about "incoherent-version-in-changelog".
               if (line.startswith('## v')):
                  nextVersionNumber = line.removeprefix('## v').replace('\n', '-1\n')
                  log.debug('Extracted version "' + nextVersionNumber.rstrip() + '" from ' + line.rstrip())
                  if (len(changes) > 0):
                     specFile.write('* ' + releaseDate + ' ' + buildConfig['CONFIG_PACKAGE_MAINTAINER'] + ' - ' +
                                    versionNumber)
                     for change in changes:
                        specFile.write('- ' + change)
                     changes = []
                  versionNumber = nextVersionNumber
                  continue
               # If this is a line starting with '* ' then it's either a new feature or a bug fix.  RPM doesn't
               # distinguish, so we just add it to the list, stripping the '* ' off the front.  EXCEPT, if the line
               # says "* None" it probably means this is a release with no new features -- just bug fixes.  So we don't
               # want to include the "* None" line!
               if (line.startswith('* ')):
                  if (line.rstrip() != '* None'):
                     changes.append(line.removeprefix('* '))
                  continue
               # If this line is '### Release Timestamp' then we want to grab the next line as the release timestamp
               if (line.startswith('### Release Timestamp')):
                  #
                  # We need to:
                  #   - take the comma out after the day of the week
                  #   - change date format from "day month year" to "month day year"
                  #   - strip the time off the end of the line
                  #   - strip the newline off the end of the line
                  # We can do all of it all in one regexp with relatively little pain(!).  Note the use of raw string
                  # notation (r prefix on string literal) to avoid the backslash plague (see
                  # https://docs.python.org/3/howto/regex.html#the-backslash-plague).
                  #
                  line = next(markdownChangeLog)
                  releaseDate = re.compile(r', (\d{1,2}) ([A-Z][a-z][a-z]) (\d\d\d\d).*\n$').sub(r' \2 \1 \3', line)
                  log.debug('Extracted date "' + releaseDate + '" from ' + line.rstrip())
                  continue
            # Once we got to the end of the input, we need to write the last change block
            if (len(changes) > 0):
               specFile.write('* ' + releaseDate + ' ' + buildConfig['CONFIG_PACKAGE_MAINTAINER'] + ' - ' +
                              versionNumber)
               for change in changes:
                  specFile.write('- ' + change)

         #
         # RPM packages want man pages to be compressed with bzip2.  Other than that, the same comments above for
         # compressing man pages for deb packages apply here.
         #
         dir_packages_rpm_man = dir_packages_rpm_buildroot.joinpath('usr').joinpath('share').joinpath('man')
         dir_packages_rpm_man1 = dir_packages_rpm_man.joinpath('man1')
         log.debug('Compressing man page')
         btUtils.abortOnRunFail(
            subprocess.run(
               ['bzip2', '--compress', dir_packages_rpm_man1.joinpath(projectName + '.1')],
               capture_output=False
            )
         )

         #
         # Run rpmbuild to build the package
         #
         # Again, as with the .deb packaging, we are just trying to build a binary package and not use all the built-in
         # magical makefiles of the full RPM build system.
         #
         # Note, per comments at
         # https://unix.stackexchange.com/questions/553169/rpmbuild-isnt-using-the-current-working-directory-instead-using-users-home
         # that you have to set the _topdir macro to stop rpmbuild wanting to put all its output under the current
         # user's home directory.  Also, we do not put quotes around this define because the subprocess module will do
         # this already (I think) because it works out there's a space in the string.  (If we do put quotes, we get an
         # error "Macro % has illegal name".)
         #
         log.info('Generating rpm package')
         btUtils.abortOnRunFail(
            subprocess.run(
               ['rpmbuild',
                '--define=_topdir ' + dir_packages_rpm.as_posix(),
                '--noclean', # Do not remove the build tree after the packages are made
                '--buildroot',
                dir_packages_rpm_buildroot.as_posix(),
                '--bb',
                dir_packages_rpm_specs.joinpath('rpm.spec').as_posix()],
               capture_output=False
            )
         )

         # rpmbuild will have put its output in RPMS/x86_64/[projectName]-[versionNumber]-1.x86_64.rpm
         dir_packages_rpm_output = dir_packages_rpm.joinpath('RPMS').joinpath('x86_64')
         rpmPackageName = projectName + '-' + buildConfig['CONFIG_VERSION_STRING'] + '-1.x86_64.rpm'

         #
         # Running rpmlint is the lintian equivalent exercise for RPMs.  Many, but by no means all, of the error and
         # warning codes are listed at https://fedoraproject.org/wiki/Common_Rpmlint_issues, though there are some
         # mistakes on that page (eg suggestion for dealing with unstripped-binary-or-object warning is "Make sure
         # binaries are executable"!)
         #
         # See packaging/linux/rpmLintfilters.toml for suppression of various rpmlint warnings (with explanations of
         # why).
         #
         # We don't however run rpmlint on old versions of Ubuntu (ie 20.04 or earlier) because they are still on
         # version 1.X of the tool and there were a lot of big changes in the 2.0 release in May 2021, including in the
         # call syntax -- see https://github.com/rpm-software-management/rpmlint/releases/tag/2.0.0 for details.
         # (Interestingly, as of that 2.0 release, rpmlint is entirely written in Python and can even be installed via
         # `pip install rpmlint` and imported as a Python module -- see https://pypi.org/project/rpmlint/.  We should
         # have a look at this, provided we can use it without messing up anything the user has already installed from
         # distro packages.)
         #
         rawVersion = btUtils.abortOnRunFail(
            subprocess.run(['rpmlint', '--version'], capture_output=True)).stdout.decode('UTF-8'
         ).rstrip()
         log.debug('rpmlint version raw: ' + rawVersion)
         # Older versions of rpmlint output eg "rpmlint version 1.11", whereas newer ones output eg "2.2.0".  With the
         # magic of regular expressions we can fix this.
         trimmedVersion = re.sub(r'^[^0-9]*', '', rawVersion).replace('_', '.')
         log.debug('rpmlint version trimmed: ' + trimmedVersion)
         rpmlintVersion = packaging.version.parse(trimmedVersion)
         log.debug('rpmlint version parsed: ' + str(rpmlintVersion))
         if (rpmlintVersion < packaging.version.parse('2.0.0')):
            log.info('Skipping invocation of rpmlint as installed version (' + str(rpmlintVersion) +
                     ') is too old (< 2.0)')
         else:
            log.info('Running rpmlint (v' + str(rpmlintVersion) +
                     ') to check the created rpm package for errors and warnings')
            btUtils.abortOnRunFail(
               subprocess.run(
                  ['rpmlint',
                   '--config',
                   dir_base.joinpath('packaging/linux'),
                   dir_packages_rpm_output.joinpath(rpmPackageName).as_posix()],
                  capture_output=False
               )
            )

         # Move the .rpm file to the top-level directory
         shutil.move(dir_packages_rpm_output.joinpath(rpmPackageName), dir_packages_platform)

         #
         # Make the checksum file
         #
         log.info('Generating checksum file for ' + rpmPackageName)
         writeSha256sum(dir_packages_platform, rpmPackageName)

      #-----------------------------------------------------------------------------------------------------------------
      #----------------------------------------------- Windows Packaging -----------------------------------------------
      #-----------------------------------------------------------------------------------------------------------------
      case 'Windows':
         log.debug('Windows Packaging')
         #
         # There are three main open-source packaging tools available for Windows:
         #
         #    - NSIS (Nullsoft Scriptable Install System) -- see https://nsis.sourceforge.io/
         #      This is widely used and reputedly simple to learn.  Actually the documentation, although OK overall, is
         #      not brilliant for beginners.  When you are trying to write your first installer script, you will find a
         #      frustrating number of errors, omissions and broken links in the documentation.  If you give up on this
         #      and take an existing working script as a starting point, the reference documentation to explain each
         #      command is not too bad.  Plus there are lots of useful titbits on Stack Overflow etc.
         #         What's less good is that the scripting language is rather primitive.  Once you start looking at
         #      variable scope and how to pass arguments to functions, you'll have a good feel for what it was like to
         #      write mainframe assembly language in the 1970s.
         #         There is one other advantage that NSIS has over Wix and Inno Setup, specifically that it is available
         #      as an MSYS2 package (mingw-w64-x86_64-nsis for 64-bit and mingw-w64-i686-nsis for 32-bit), whereas the
         #      others are not.  This makes it easier to script installations, including for the automated builds on
         #      GitHub.
         #
         #    - WiX -- see https://wixtoolset.org/ and https://github.com/wixtoolset/
         #      This is apparently used by a lot of Microsoft's own products and is supposedly pretty robust.  Looks
         #      like you configure/script it with XML and PowerShell.  Most discussion of it says you really first need
         #      to have a good understanding of Windows Installer (https://en.wikipedia.org/wiki/Windows_Installer) and
         #      its MSI package format.  There is a 260 page book called "The Definitive Guide to Windows Installer"
         #      which either is or isn't beginner-friendly depending on who you ask but, either way is about 250 pages
         #      more than I want to have to know about Windows package installation.  If we decided we _needed_ to
         #      produce MSI installers though, this would be the only choice.
         #
         #    - Inno Setup -- see https://jrsoftware.org/isinfo.php and https://github.com/jrsoftware/issrc
         #      Has been around for ages, but is less widely used than NSIS.  Basic configuration is supposedly simpler
         #      than NSIS, as it's based on an INI file (https://en.wikipedia.org/wiki/INI_file), but you also, by
         #      default, have a bit less control over how the installer works.  If you do need to script something you
         #      have to do it in Pascal, so a trip back to the 1980s rather than the 1970s.
         #
         # For the moment, we're sticking with NSIS, which is the devil we know, aka what we've historically used.
         #
         # In the past, we built only 32-bit packages (i686 architecture) on Windows because of problems getting 64-bit
         # versions of NSIS plugins to work.  However, we now invoke NSIS without plugins, so the 64-bit build seems to
         # be working.
         #
         # As of January 2024, some of the 32-bit MSYS2 packages/groups we were previously relying on previously are no
         # longer available.  So now, we only build 64-bit packages (x86_64 architecture) on Windows.
         #

         #
         # As mentioned above, not all information about what Meson does is readily exportable.   In particular, I can
         # find no simple way to get the actual directory that a file was installed to.  Eg, on Windows, in an MSYS2
         # environment, the main executable will be in mbuild/packages/windows/msys64/mingw32/bin/ or similar.  The
         # beginning (mbuild/packages/windows) and the end (bin) are parts we specify, but the middle bit
         # (msys64/mingw32) is magicked up by Meson and not explicitly exposed to build script commands.
         #
         # Fortunately, we can just search for a directory called bin inside the platform-specific packaging directory
         # and we'll have the right thing.
         #
         # (An alternative approach would be to invoke meson with the --bindir parameter to manually choose the
         # directory for the executable.)
         #
         packageBinDirList = glob.glob('./**/bin/', root_dir=dir_packages_platform.as_posix(), recursive=True)
         if (len(packageBinDirList) == 0):
            log.critical(
               'Cannot find bin subdirectory of ' + dir_packages_platform.as_posix() + ' packaging directory'
            )
            exit(1)
         if (len(packageBinDirList) > 1):
            log.warning(
               'Found more than one bin subdirectory of ' + dir_packages_platform.as_posix() +
               ' packaging directory: ' + '; '.join(packageBinDirList) + '.  Assuming first is the one we need'
            )

         dir_packages_win_bin = dir_packages_platform.joinpath(packageBinDirList[0])
         log.debug('Package bin dir: ' + dir_packages_win_bin.as_posix())

         #
         # We could do the same search for data and doc directories, but we happen to know that they should just be
         # sibling directories of the bin directory we just found.
         #
         dir_packages_win_data = dir_packages_win_bin.parent.joinpath('data')
         dir_packages_win_doc  = dir_packages_win_bin.parent.joinpath('doc')

         #
         # Now we have to deal with shared libraries.  Windows does not have a built-in package manager and it's not
         # realistic for us to require end users to install and use one.  So, any shared library that we cannot
         # statically link into the application needs to be included in the installer.  This mainly applies to Qt.
         # (Although you can, in principle, statically link against Qt, it requires downloading the entire Qt source
         # code and doing a custom build.)  Fortunately, Qt provides a handy utility called windeployqt that should do
         # most of the work for us.
         #
         # Per https://doc.qt.io/qt-6/windows-deployment.html, the windeployqt executable creates all the necessary
         # folder tree "containing the Qt-related dependencies (libraries, QML imports, plugins, and translations)
         # required to run the application from that folder".
         #
         log.debug('Running windeployqt')
         previousWorkingDirectory = pathlib.Path.cwd().as_posix()
         os.chdir(dir_packages_win_bin)
         btUtils.abortOnRunFail(
            subprocess.run(['windeployqt',
                            '--verbose', '2',        # 2 is the maximum
                            projectName + '.exe'],
                           capture_output=False)
         )
         os.chdir(previousWorkingDirectory)

         #
         # We're not finished with shared libraries.  Although windeployqt is theoretically capable of detecting all the
         # shared libraries we need, including non-Qt ones, it doesn't, in practice, seem to be that good on the non-Qt
         # bit.  And although, somewhere in the heart of the Meson implementation, you would think it would or could
         # know the full paths to the shared libraries on which we depend, this is not AFAICT extractable in the
         # meson.build script.  So, here, we have a list of libraries that we know we depend on and we search for them
         # in the paths listed in the PATH environment variable.  It's a bit less painful than you might think to
         # construct and maintain this list of libraries, because, for the most part, if you miss a needed DLL from the
         # package, Windows will give you an error message at start-up telling you which DLL(s) it needed but could not
         # find.  (There are also various platform-specific free-standing tools that claim to examine an executable and
         # tell you what shared libraries it depends on.  None that I know of is easy to install in an automated way in
         # MSYS2 however.)
         #
         # We assume that the library 'foo' has a dll called 'libfoo.dll' or 'libfoo-X.dll' or 'libfooX.dll' where X is
         # a (possibly multi-digit) version number present on some, but not all, libraries.  If we find more matches
         # than we were expecting, we log a warning and just include everything we found.  (Sometimes we include the
         # version number in the library name because we really are looking for a specific version or there are always
         # multiple versions)  It's not super pretty, but it should work.
         #
         # Just to keep us on our toes, the Python os module has two similarly-named but different things:
         #    - os.pathsep is the separator between paths (usually ';' or ':') eg in the PATH environment variable
         #    - os.sep is the separator between directories (usually '/' or '\\') in a path
         #
         # The comments below about the source of libraries are just FYI.  In almost all cases, we are actually
         # installing these things on the build machine via pacman, so we don't have to go directly to the upstream
         # project.
         #
         pathsToSearch = os.environ['PATH'].split(os.pathsep)
         for extraLib in [
            'libbrotlicommon',      # Brotli compression -- see https://en.wikipedia.org/wiki/Brotli
            'libbrotlidec',         # Brotli compression
            'libbrotlienc',         # Brotli compression
            'libbz2',               # BZip2 compression -- see https://en.wikipedia.org/wiki/Bzip2
            'libdouble-conversion', # See https://github.com/google/double-conversion
            'libfreetype',          # See https://freetype.org/
            #
            # 32-bit and 64-bit MinGW use different exception handling (see
            # https://sourceforge.net/p/mingw-w64/wiki2/Exception%20Handling/) hence the different naming of libgcc in
            # the 32-bit and 64-bit environments.
            #
#            'libgcc_s_dw2',    # 32-bit GCC library
            'libgcc_s_seh',    # 64-bit GCC library
            'libglib-2.0',
            'libgraphite',
            'libharfbuzz',   # HarfBuzz text shaping engine -- see https://github.com/harfbuzz/harfbuzz
            'libiconv',      # See https://www.gnu.org/software/libiconv/
            'libicudt',      # Part of International Components for Unicode
            'libicuin',      # Part of International Components for Unicode
            'libicuuc',      # Part of International Components for Unicode
            'libintl',       # See https://www.gnu.org/software/gettext/
            'libmd4c',       # Markdown for C -- see https://github.com/mity/md4c
            'libpcre2-8',    # Perl Compatible Regular Expressions
            'libpcre2-16',   # Perl Compatible Regular Expressions
            'libpcre2-32',   # Perl Compatible Regular Expressions
            'libpng16',      # Official PNG reference library -- see http://www.libpng.org/pub/png/libpng.html
            'libsqlite3',    # Need this IN ADDITION to bin/sqldrivers/qsqlite.dll, which gets installed by windeployqt
            'libstdc++',
            'libwinpthread',
            'libxalan-c',
            'libxalanMsg',
            'libxerces-c-3',
            'libzstd',       # ZStandard (aka zstd) = fast lossless compression algorithm
            'zlib',          # ZLib compression library
         ]:
            found = False
            for searchDir in pathsToSearch:
               # We do a glob match to get approximate matches and then filter it with a regular expression for exact
               # ones
               matches = []
               globMatches = glob.glob(extraLib + '*.dll', root_dir=searchDir, recursive=False)
               for globMatch in globMatches:
                  # We need to remove the first part of the glob match before doing a regexp match because we don't want
                  # the first part of the filename to be treated as a regular expression.  In particular, this would be
                  # a problem for 'libstdc++'!
                  suffixOfGlobMatch = globMatch.removeprefix(extraLib)
                  # On Python 3.11 or later, we would write flags=re.NOFLAG instead of flags=0
                  if re.fullmatch(re.compile('-?[0-9]*.dll'), suffixOfGlobMatch, flags=0):
                     matches.append(globMatch)
               numMatches = len(matches)
               if (numMatches > 0):
                  log.debug('Found ' + str(numMatches) + ' match(es) for ' + extraLib + ' in ' + searchDir)
                  if (numMatches > 1):
                     log.warning('Found more matches than expected (' + str(numMatches) + ' ' +
                                 'instead of 1) when searching for library "' + extraLib + '".  This is not an ' +
                                 'error, but means we are possibly shipping additional shared libraries that we '+
                                 'don\'t need to.')
                  for match in matches:
                     fullPathOfMatch = pathlib.Path(searchDir).joinpath(match)
                     log.debug('Copying ' + fullPathOfMatch.as_posix() + ' to ' + dir_packages_win_bin.as_posix())
                     shutil.copy2(fullPathOfMatch, dir_packages_win_bin)
                  found = True
                  break;
            if (not found):
               log.critical('Could not find '+ extraLib + ' library in PATH ' + os.environ['PATH'])
               exit(1)

         # Copy the NSIS installer script to where it belongs
         shutil.copy2(dir_build.joinpath('NsisInstallerScript.nsi'), dir_packages_platform)

         # We change into the packaging directory and invoke the NSIS Compiler (aka MakeNSIS.exe)
         os.chdir(dir_packages_platform)
         log.debug('Working directory now ' + pathlib.Path.cwd().as_posix())
         btUtils.abortOnRunFail(
            # FYI, we don't need it here, but if you run makensis from the MSYS2 command line (Mintty), you need double
            # slashes on the options (//V4 instead of /V4 etc).
            subprocess.run(
               [
                  'MakeNSIS.exe', # 'makensis' would also work on MSYS2
                  '/V4',          # Max verbosity/logging
                  # Variables coming from this script are passed in as command-line defines.  Fortunately there aren't
                  # too many of them.
                  '/DBT_PACKAGING_BIN_DIR="'  + dir_packages_win_bin.as_posix()  + '"',
                  '/DBT_PACKAGING_DATA_DIR="' + dir_packages_win_data.as_posix() + '"',
                  '/DBT_PACKAGING_DOC_DIR="'  + dir_packages_win_doc.as_posix()  + '"',
                  'NsisInstallerScript.nsi',
               ],
               capture_output=False
            )
         )

         #
         # Make the checksum file TODO
         #
         winInstallerName = capitalisedProjectName + ' ' + buildConfig['CONFIG_VERSION_STRING'] + ' Installer.exe'
         log.info('Generating checksum file for ' + winInstallerName)
         writeSha256sum(dir_packages_platform, winInstallerName)

      #-----------------------------------------------------------------------------------------------------------------
      #------------------------------------------------- Mac Packaging -------------------------------------------------
      #-----------------------------------------------------------------------------------------------------------------
      case 'Darwin':
         log.debug('Mac Packaging')
         #
         # See https://stackoverflow.com/questions/1596945/building-osx-app-bundle for essential info on building Mac
         # app bundles.  Also https://mesonbuild.com/Creating-OSX-packages.html suggests how to do this with Meson,
         # though it's mostly through having Meson call shell scripts, so I think we're better off sticking to this
         # Python script.
         #
         # https://developer.apple.com/library/archive/documentation/CoreFoundation/Conceptual/CFBundles/BundleTypes/BundleTypes.html
         # is the "official" Apple info about the directory structure.
         #
         # To create a Mac app bundle , we create the following directory structure, where items marked ✅ are copied as
         # is from the tree generated by meson install with --destdir option, those marked 🟢 are handled by
         # `macdeployqt`, and those marked ❇ are ones we need to relocate, generate or modify ourselves.
         #
         # (When working on this bit, use ❌ for things that are generated automatically but not actually needed, and ✴
         # for things we still need to add.)
         #    [projectName]_[versionNumber].app
         #    └── Contents
         #        ├── Info.plist ❇  <── "Information property list" file = required configuration information (in XML)
         #        ├── Frameworks  <── Contains any private shared libraries and frameworks used by the executable
         #        │   ├── QtCore.framework * NB: Directory and its contents *  🟢
         #        │   ├── [Other Qt .framework directories and their contents] 🟢
         #        │   ├── libfreetype.6.dylib    🟢
         #        │   ├── libglib-2.0.0.dylib    🟢
         #        │   ├── libgthread-2.0.0.dylib 🟢
         #        │   ├── libintl.8.dylib        🟢
         #        │   ├── libjpeg.8.dylib        🟢
         #        │   ├── libpcre2-16.0.dylib    🟢
         #        │   ├── libpcre2-8.0.dylib     🟢
         #        │   ├── libpng16.16.dylib      🟢
         #        │   ├── libsharpyuv.0.dylib    🟢
         #        │   ├── libtiff.5.dylib        🟢
         #        │   ├── libwebp.7.dylib        🟢
         #        │   ├── libwebpdemux.2.dylib   🟢
         #        │   ├── libwebpmux.3.dylib     🟢
         #        │   ├── libxalan-c.112.dylib   🟢
         #        │   ├── libxerces-c-3.2.dylib  🟢
         #        │   ├── libzstd.1.dylib        🟢
         #        │   └── libxalanMsg.112.dylib  ❇ ✴
         #        ├── MacOS
         #        │   └── [capitalisedProjectName] ❇  <── the executable
         #        ├── Plugins  <── Contains loadable bundles that extend the basic features of the application
         #        │   ├── audio
         #        │   │   └── libqtaudio_coreaudio.dylib 🟢
         #        │   ├── bearer
         #        │   │   └── libqgenericbearer.dylib 🟢
         #        │   ├── iconengines
         #        │   │   └── libqsvgicon.dylib 🟢
         #        │   ├── imageformats
         #        │   │   ├── libqgif.dylib     🟢
         #        │   │   ├── libqicns.dylib    🟢
         #        │   │   ├── libqico.dylib     🟢
         #        │   │   ├── libqjpeg.dylib    🟢
         #        │   │   ├── libqmacheif.dylib 🟢
         #        │   │   ├── libqmacjp2.dylib  🟢
         #        │   │   ├── libqsvg.dylib     🟢
         #        │   │   ├── libqtga.dylib     🟢
         #        │   │   ├── libqtiff.dylib    🟢
         #        │   │   ├── libqwbmp.dylib    🟢
         #        │   │   └── libqwebp.dylib    🟢
         #        │   ├── mediaservice
         #        │   │   ├── libqavfcamera.dylib          🟢
         #        │   │   ├── libqavfmediaplayer.dylib     🟢
         #        │   │   └── libqtmedia_audioengine.dylib 🟢
         #        │   ├── platforms
         #        │   │   └── libqcocoa.dylib 🟢
         #        │   ├── printsupport
         #        │   │   └── libcocoaprintersupport.dylib 🟢
         #        │   ├── sqldrivers
         #        │   │   ├── libqsqlite.dylib  🟢
         #        │   │   ├── libqsqlodbc.dylib ✴  Not sure we need this one, but it got shipped with Brewtarget 2.3
         #        │   │   └── libqsqlpsql.dylib ✴
         #        │   ├── styles
         #        │   │  └── libqmacstyle.dylib 🟢
         #        │   └── virtualkeyboard
         #        │       ├── libqtvirtualkeyboard_hangul.dylib  🟢
         #        │       ├── libqtvirtualkeyboard_openwnn.dylib 🟢
         #        │       ├── libqtvirtualkeyboard_pinyin.dylib  🟢
         #        │       ├── libqtvirtualkeyboard_tcime.dylib   🟢
         #        │       └── libqtvirtualkeyboard_thai.dylib    🟢
         #        └── Resources
         #            ├── [capitalisedProjectName]Icon.icns ✅  <── Icon file
         #            ├── DefaultData.xml   ✅
         #            ├── default_db.sqlite ✅
         #            ├── en.lproj        <── Localized resources
         #            │   ├── COPYRIGHT ✅
         #            │   └── README.md ✅
         #            ├── qt.conf ✅
         #            ├── sounds
         #            │   └── [All the filesToInstall_sounds .wav files] ✅
         #            └── translations_qm
         #                └── [All the .qm files generated by qt.compile_translations] ✅
         #
         # This will ultimately get bundled up into a disk image (.dmg) file.
         #

         #
         # Make the top-level directories
         #
         log.debug('Creating Mac app bundle top-level directories')
         macBundleDirName = projectName + '_' + buildConfig['CONFIG_VERSION_STRING'] + '.app'
         dir_packages_mac = dir_packages_platform.joinpath(macBundleDirName).joinpath('Contents')
         dir_packages_mac_bin = dir_packages_mac.joinpath('MacOS')
         dir_packages_mac_rsc = dir_packages_mac.joinpath('Resources')
         dir_packages_mac_frm = dir_packages_mac.joinpath('Frameworks')
         dir_packages_mac_plg = dir_packages_mac.joinpath('Plugins')
         os.makedirs(dir_packages_mac_bin) # This will also automatically create parent directories
         os.makedirs(dir_packages_mac_frm)
         os.makedirs(dir_packages_mac_plg)
         # Rather than create dir_packages_mac_rsc directly, it's simplest to copy the whole Resources tree from
         # mbuild/mackages/darwin/usr/local/Contents/Resources, as we want everything that's inside it
         log.debug('Copying Resources')
         shutil.copytree(dir_packages_platform.joinpath('usr/local/Contents/Resources'), dir_packages_mac_rsc)

         # Copy the Information Property List file to where it belongs
         log.debug('Copying Information Property List file')
         shutil.copy2(dir_build.joinpath('Info.plist').as_posix(), dir_packages_mac)

         # Because Meson is geared towards local installs, in the mbuild/mackages/darwin directory, it is going to have
         # placed the executable in the usr/local/bin subdirectory.  Copy it to the right place.
         log.debug('Copying executable')
         shutil.copy2(dir_packages_platform.joinpath('usr/local/bin').joinpath(capitalisedProjectName).as_posix(),
                      dir_packages_mac_bin)

         #
         # The macdeployqt executable shipped with Qt does for Mac what windeployqt does for Windows -- see
         # https://doc.qt.io/qt-6/macos-deployment.html#the-mac-deployment-tool
         #
         # At first glance, you might thanks that, with a few name changes, we might share all the bt code for
         # macdeployqt and windeployqt.  However, the two programs share _only_ a top-level goal ("automate the process
         # of creating a deployable [folder / application bundle] that contains [the necessary Qt dependencies]" - ie so
         # that the end user does not have to install Qt to run our software).  They have completely different
         # implementations and command line options, so it would be unhelpful to try to treat them identically.
         #
         # With the verbose logging on, you can see that macdeployqt is calling:
         #    - otool (see https://www.unix.com/man-page/osx/1/otool/) to get information about which libraries etc the
         #      executable depends on
         #    - install_name_tool (see https://www.unix.com/man-page/osx/1/install_name_tool/) to change the paths in
         #      which the executable looks for a library
         #    - strip (see https://www.unix.com/man-page/osx/1/strip/) to remove symbols from shared libraries
         #
         # As discussed at https://stackoverflow.com/questions/2809930/macdeployqt-and-third-party-libraries, there are
         # usually cases where you have to do some of the same work by hand because macdeployqt doesn't automatically
         # detect all the dependencies.  One example of this is that, if a shared library depends on another shared
         # library then macdeployqt won't detect it, because it does not recursively run its dependency checking.
         #
         # For us, macdeployqt does seem to cover almost all the shared libraries and frameworks we need, including
         # those that are not part of Qt.  The exceptions are:
         #    - libxalanMsg -- a library that libxalan-c uses (so an indirect rather than direct dependency)
         #    - libqsqlpsql.dylib -- which would be needed for any user that wants to use PostgreSQL instead of SQLite
         #
         previousWorkingDirectory = pathlib.Path.cwd().as_posix()
         log.debug('Running otool before macdeployqt')
         os.chdir(dir_packages_mac_bin)
         otoolOutputExe = btUtils.abortOnRunFail(
            subprocess.run(['otool',
                            '-L',
                            capitalisedProjectName],
                           capture_output=True)
         ).stdout.decode('UTF-8')
         log.debug('Output of `otool -L' + capitalisedProjectName + '`: ' + otoolOutputExe)
         #
         # The output from otool at this stage will be along the following lines:
         #
         #    [capitalisedProjectName]:
         #       /usr/local/opt/qt@5/lib/QtCore.framework/Versions/5/QtCore (compatibility version 5.15.0, current version 5.15.8)
         #       /usr/local/opt/qt@5/lib/QtGui.framework/Versions/5/QtGui (compatibility version 5.15.0, current version 5.15.8)
         #       /usr/local/opt/qt@5/lib/QtMultimedia.framework/Versions/5/QtMultimedia (compatibility version 5.15.0, current version 5.15.8)
         #       /usr/local/opt/qt@5/lib/QtNetwork.framework/Versions/5/QtNetwork (compatibility version 5.15.0, current version 5.15.8)
         #       /usr/local/opt/qt@5/lib/QtPrintSupport.framework/Versions/5/QtPrintSupport (compatibility version 5.15.0, current version 5.15.8)
         #       /usr/local/opt/qt@5/lib/QtSql.framework/Versions/5/QtSql (compatibility version 5.15.0, current version 5.15.8)
         #       /usr/local/opt/qt@5/lib/QtWidgets.framework/Versions/5/QtWidgets (compatibility version 5.15.0, current version 5.15.8)
         #       /usr/local/opt/xerces-c/lib/libxerces-c-3.2.dylib (compatibility version 0.0.0, current version 0.0.0)
         #       /usr/local/opt/xalan-c/lib/libxalan-c.112.dylib (compatibility version 112.0.0, current version 112.0.0)
         #       /usr/lib/libc++.1.dylib (compatibility version 1.0.0, current version 1300.36.0)
         #       /usr/lib/libSystem.B.dylib (compatibility version 1.0.0, current version 1319.0.0)
         #
         # After running `macdeployqt`, all the paths for non-system libraries will be changed to ones beginning
         # '@loader_path/../Frameworks/', as will be seen from the subsequent output of running `otool`.
         #
         # We want to grab:
         #   - the directory containing libxalan-c, as that's the same directory in which we should find libxalanMsg
         #   - information that would allow us to find libqsqlpsql.dylib .:TODO:. Still to work out how to do this.  For
         #     now, I think that means users requiring PostgreSQL support on MacOS will need to build the app from
         #     source.
         #
         xalanDir = ''
         xalanLibName = ''
         xalanMatch =  re.search(r'^\s*(\S+/)(libxalan-c\S*.dylib)', otoolOutputExe, re.MULTILINE)
         if (xalanMatch):
            # The [1] index gives us the first parenthesized subgroup of the regexp match, which in this case should be
            # the directory path to libxalan-c.xxx.dylib
            xalanDir = xalanMatch[1]
            xalanLibName = xalanMatch[2]
         else:
            log.warning(
               'Could not find libxalan dependency in ' + capitalisedProjectName +
               ' so assuming /usr/local/opt/xalan-c/lib/'
            )
            xalanDir = '/usr/local/opt/xalan-c/lib/'
            xalanLibName = 'libxalan-c.112.dylib'
         log.debug('xalanDir: ' + xalanDir + '; contents:')
         btUtils.abortOnRunFail(subprocess.run(['ls', '-l', xalanDir], capture_output=False))

         #
         # Strictly speaking, we should look at every /usr/local/opt/.../*.dylib dependency of our executable, and run
         # each of those .dylib files through otool to get its dependencies, then repeat until we find no new
         # dependencies.  Then we should ensure each dependency is copied into the app bundle and whatever depends on it
         # knows where to find it etc.  Pretty soon we'd have ended up reimplementing macdeployqt.  Fortunately, in
         # practice, for Xalan, it suffices to grab libxalanMsg and put it in the same directory in the bundle as
         # libxalanc.
         #
         # We use otool to get the right name for libxalanMsg, which is typically listed as a relative path dependency
         # eg '@rpath/libxalanMsg.112.dylib'.
         #
         # Per https://www.mikeash.com/pyblog/friday-qa-2009-11-06-linking-and-install-names.html:
         #
         #    @executable_path - will expand at run time to the absolute path of the app bundle's executable directory,
         #                       ie [projectName]_[versionNumber].app/Contents/MacOS for us
         #
         #    @loader_path     - will expand at run time to the absolute path of whatever is loading the library,
         #                       typically either the executable directory (if it's the executable loading the library
         #                       directly) or, for us, the [projectName]_[versionNumber].app/Contents/Frameworks
         #                       directory if it's another shared library requesting the load
         #
         #    @rpath           - means search a list of locations specified at the point the application was linked (by
         #                       means of the -rpath linker flag), so, eg, including
         #                       '-rpath @executable_path/../Frameworks' at link time means, for us, that
         #                       [projectName]_[versionNumber].app/Contents/Frameworks is one of the places to search
         #                       when @rpath is specified
         #
         log.debug('Running otool -L on ' + xalanLibName)
         otoolOutputXalan = btUtils.abortOnRunFail(
            subprocess.run(['otool',
                            '-L',
                            xalanDir + xalanLibName],
                           capture_output=True)
         ).stdout.decode('UTF-8')
         log.debug('Output of `otool -L' + xalanDir + xalanLibName + '`: ' + otoolOutputXalan)
         xalanMsgLibName = ''
         xalanMsgMatch =  re.search(r'^\s*(\S+/)(libxalanMsg\S*.dylib)', otoolOutputXalan, re.MULTILINE)
         if (xalanMsgMatch):
            xalanMsgLibName = xalanMsgMatch[2]
         else:
            log.warning(
               'Could not find libxalanMsg dependency in ' + xalanDir + xalanLibName +
               ' so assuming libxalanMsg.112.dylib'
            )
            xalanMsgLibName = 'libxalanMsg.112.dylib'
         log.debug('Copying ' + xalanDir + xalanMsgLibName + ' to ' + dir_packages_mac_frm.as_posix())
         shutil.copy2(xalanDir + xalanMsgLibName, dir_packages_mac_frm)

###         #
###         # Let's copy libxalan-c.112.dylib to where it needs to go and hope that macdeployqt does not overwrite it
###         #
###         shutil.copy2(xalanDir + xalanLibName, dir_packages_mac_frm)
###
###         #
###         # We need to fix up libxalan-c.112.dylib so that it looks for libxalanMsg.112.dylib in the right place.  Long
###         # story short, this means changing '@rpath/libxalanMsg.112.dylib' to '@loader_path/libxalanMsg.112.dylib'
###         #
###         os.chdir(dir_packages_mac_frm)
###         btUtils.abortOnRunFail(
###            subprocess.run(['install_name_tool',
###                            '-change',
###                            xalanMsgLibName,
###                            '@loader_path/' + xalanMsgLibName],
###                           capture_output=False)
###         )

         #
         # Now let macdeployqt do most of the heavy lifting
         #
         # Note that it is best to run macdeployqt in the directory that contains the [projectName]_[versionNumber].app
         # folder (otherwise, eg, the dmg name it creates will be wrong, as explained at
         # https://doc.qt.io/qt-6/macos-deployment.html.
         #
         # In a previous iteration of this script, I skipped the -dmg option and tried to build the disk image with
         # dmgbuild (code at https://github.com/dmgbuild/dmgbuild, docs at
         # https://dmgbuild.readthedocs.io/en/latest/index.html).  The advantages of this would be that it would make it
         # possible to do further fix-up work on the directory tree (if needed) and, potentially, give us more control
         # over the disk image (eg to specify an icon for it).  However, it seemed to be fiddly to get it to work.  It's
         # a lot simpler to let macdeployqt create the disk image, and we currently don't think we need to do further
         # fix-up work after it's run.  A custom icon on the disk image would be nice, but is far from essential.
         #
         log.debug('Running macdeployqt')
         os.chdir(dir_packages_platform)
         btUtils.abortOnRunFail(
            #
            # Note that app bundle name has to be the first parameter and options come afterwards.
            # The -executable argument is to automatically alter the search path of the executable for the Qt libraries
            # (ie so the executable knows where to find them inside the bundle)
            #
            subprocess.run(['macdeployqt',
                            macBundleDirName,
                            '-verbose=2',        # 0 = no output, 1 = error/warning (default), 2 = normal, 3 = debug
                            '-executable=' + macBundleDirName + '/Contents/MacOS/' + capitalisedProjectName,
                            '-dmg'],
                           capture_output=False)
         )

         #
         # The result of specifying the `-dmg' flag should be a [projectName]_[versionNumber].dmg file
         #
         log.debug('Directory tree after running macdeployqt')
         btUtils.abortOnRunFail(subprocess.run(['tree', '-sh'], capture_output=False))
         dmgFileName = macBundleDirName.replace('.app', '.dmg')

         log.debug('Running otool on ' + capitalisedProjectName + ' executable after macdeployqt')
         os.chdir(dir_packages_mac_bin)
         btUtils.abortOnRunFail(subprocess.run(['otool', '-L', capitalisedProjectName], capture_output=False))
         btUtils.abortOnRunFail(subprocess.run(['otool', '-l', capitalisedProjectName], capture_output=False))
         log.debug('Running otool on ' + xalanLibName + ' library after macdeployqt')
         os.chdir(dir_packages_mac_frm)
         btUtils.abortOnRunFail(subprocess.run(['otool', '-L', xalanLibName], capture_output=False))

         log.info('Created ' + dmgFileName + ' in directory ' + dir_packages_platform.as_posix())

         #
         # We can now mount the disk image and check its contents.  (I don't think we can modify the contents though.)
         #
         # By default, a disk image called foobar.dmg will get mounted at /Volumes/foobar.
         #
         log.debug('Running hdiutil to mount ' + dmgFileName)
         os.chdir(dir_packages_platform)
         btUtils.abortOnRunFail(
            subprocess.run(
               ['hdiutil', 'attach', '-verbose', dmgFileName]
            )
         )
         mountPoint = '/Volumes/' + dmgFileName.replace('.dmg', '')
         log.debug('Directory tree of disk image')
         btUtils.abortOnRunFail(
            subprocess.run(['tree', '-sh', mountPoint], capture_output=False)
         )
         log.debug('Running hdiutil to unmount ' + mountPoint)
         os.chdir(dir_packages_platform)
         btUtils.abortOnRunFail(
            subprocess.run(
               ['hdiutil', 'detach', '-verbose', mountPoint]
            )
         )

         #
         # Make the checksum file
         #
         log.info('Generating checksum file for ' + dmgFileName)
         writeSha256sum(dir_packages_platform, dmgFileName)

         os.chdir(previousWorkingDirectory)

      case _:
         log.critical('Unrecognised platform: ' + platform.system())
         exit(1)

   # If we got this far, everything must have worked
   print()
   print('⭐ Packaging complete ⭐')
   print('See:')
   print('   ' + dir_packages_platform.as_posix() + ' for binaries')
   print('   ' + dir_packages_source.as_posix() + ' for source')
   return

#-----------------------------------------------------------------------------------------------------------------------
# .:TBD:.  Let's see if we can do a .deb package
#-----------------------------------------------------------------------------------------------------------------------
def doDebianPackage():
   return

#-----------------------------------------------------------------------------------------------------------------------
# Act on command line arguments
#-----------------------------------------------------------------------------------------------------------------------
# See above for parsing
match args.subCommand:

   case 'setup':
      doSetup(setupOption = args.setupOption)

   case 'package':
      doPackage()

   # If we get here, it's a coding error as argparse should have already validated the command line arguments
   case _:
      log.error('Unrecognised command "' + command + '"')
      exit(1)
