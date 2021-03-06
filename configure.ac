AC_INIT(include/libu8/libu8.h)
AC_PREFIX_PROGRAM(autoconf)
m4_include([etc/ax_pthread.m4])
threads_default="yes"
ifmacosx="#"
shared_default="yes"
shared_suffix="so"
SHARED_LIB="libu8.so libu8stdio.so libu8fns.so libu8io.so libu8data.so"
CLEAN="rm -f"
U8MAJOR="2"
U8MINOR="3"
U8RELEASE="1"
U8VERSION="2.3.1"
LD="ld"
MKSTATIC="\$(LD) -r -o"
I18N=""
INSTALLI18N=""
SUDO=""
TAGS="TAGS"
DOCS="docs"
INSTALLDOCS="install-docs"
BUILDRPM="lsbrpmbuild"
doi18n="no"
build_shared="shared-libs"
build_static="static-libs"
test_shared="test-shared"
test_static="test-static"
install_shared="install-so"
install_static="install-static"
libc_pthreads="no"
libc_dlopen="no"
syslog_static="syslog-static"
syslog_shared="syslog-shared"
suffix=""
have_constructor_attributes="yes"
archflags=""
definefileinfo=""
versionsh=""
doxygen=""

AC_CONFIG_AUX_DIR(etc)
AC_CONFIG_HEADER(include/libu8/u8source.h include/libu8/config.h)
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

case $target_os in
  linux*)
   AC_DEFINE(U8_RUSAGE_PROC_PATCH,1)
   ;;
  darwin*)
   MKSTATIC="libtool -static -o ";
#   if test "x$CC" = "xgcc"; then LIBS="$LIBS -lgcc"; fi
   shared_suffix="dylib";
   install_shared="install-dylib";
   ifmacosx="";
   CFLAGS="-D_DARWIN_C_SOURCE $CFLAGS"
   AC_DEFINE(DESTRUCTIVE_POLL,1)
   ;;
esac  

AC_SUBST(LD)
AC_SUBST(CLEAN)
AC_SUBST(TAGS)
AC_SUBST(SHARED_LIB)
AC_SUBST(SHARED_LIBV)
AC_SUBST(U8MAJOR)
AC_SUBST(U8MINOR)
AC_SUBST(U8RELEASE)
AC_SUBST(U8VERSION)
AC_SUBST(MKSTATIC)
AC_SUBST(SUDO)
AC_SUBST(shared_suffix)
AC_SUBST(suffix)
AC_SUBST(test_shared)
AC_SUBST(test_static)
AC_SUBST(build_shared)
AC_SUBST(build_static)
AC_SUBST(install_shared)
AC_SUBST(install_static)
AC_SUBST(syslog_shared)
AC_SUBST(syslog_static)
AC_SUBST(archflags)
AC_SUBST(definefileinfo)
AC_SUBST(versionsh)
AC_SUBST(I18N)
AC_SUBST(INSTALLI18N)
AC_SUBST(doxygen)
AC_SUBST(DOCS)
AC_SUBST(INSTALLDOCS)

AC_ARG_WITH(asan,--with-asan,,with_asan="no")
if test "x$with_asan" = "xyes"; then
   CC=clang
   CFLAGS="-fsanitize=address -fno-omit-frame-pointer -O0 -g $CFLAGS"
   LD=clang
   LDFLAGS="-g -fsanitize=address $LDFLAGS"
   build_static=""
   test_static=""
   install_static=""
fi

AC_PROG_CC([cc gcc])
AC_USE_SYSTEM_EXTENSIONS
AC_PROG_INSTALL
AC_PROG_RANLIB
AC_AIX

AC_ARG_WITH(encodings_dir,--with-encodings-dir pathname,,with_encodings_dir=$prefix/share/libu8/encodings)
AC_DEFINE_UNQUOTED(U8_ENCODINGS_DIR,"$with_encodings_dir")

dnl For specifying default -arch flags for OSX
AC_ARG_WITH(arch,--with[out]-arch build with particular --arch flags,,with_arch="")
if test -z $with_arch; then
  archflags=""
elif test $with_arch = "all"; then
  archflags="-arch i386 -arch x86_64 -arch ppc -arch ppc64";
elif test $with_arch = "all86"; then
  archflags="-arch i386 -arch x86_64";
elif test $with_arch = "allppc"; then
  archflags="-arch ppc -arch ppc64";
else archflags=$with_arch
fi

AC_ARG_ENABLE(shared,--en[able]-shared Build with/without shared libraries,,enable_shared=$shared_default)
if test $target = "i686-pc-mingw32"; then
  CFLAGS="-DU8_DLL=1 $CFLAGS"
elif test $enable_shared = "yes"; then
  CFLAGS="-fPIC $CFLAGS"
else
  SHARED_LIB=
fi

AC_CHECK_PROGS(etags,"etags","na")
AC_CHECK_PROGS(doxygen,"doxygen","na")
AC_CHECK_PROGS(buildrpm,"lsb-rpmbuild","rpmbuild","lsb-rpm","na")

if test $etags = "na"; then
   TAGS=""
fi
if test "x$doxygen" = "xna"; then
   DOCS=""
   INSTALLDOCS=""
fi

gcc_version=`gcc -dumpversion`

case $gcc_version in
  4*)
   CFLAGS="-Wno-pointer-sign -Wimplicit -fno-strict-aliasing $CFLAGS"
  ;;
esac

if test $target = "i686-pc-mingw32"; then
   shared_suffix="dll"
   install_shared="install-dll"
   LIBS="$LIBS -lwsock32"
fi

AC_DEFINE(FD_WORDS_ARE_ALIGNED,1)
AC_TYPE_UID_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_PID_T
AC_TYPE_MODE_T
AC_C_INLINE
AC_C_BIGENDIAN
AC_CHECK_SIZEOF(int,4)
AC_CHECK_SIZEOF(long,4)
AC_CHECK_SIZEOF(long long,8)
AC_CHECK_SIZEOF(void *,4)

AC_HEADER_STDC
AC_HEADER_STAT

AC_CHECK_HEADERS(strings.h netdb.h sys/types.h)
AC_CHECK_HEADERS(sys/stat.h unistd.h pwd.h grp.h fcntl.h poll.h sys/poll.h)
AC_CHECK_HEADERS(sys/socket.h sys/select.h sys/un.h netdb.h)
AC_CHECK_HEADERS(netinet/in.h netinet/tcp.h)
AC_CHECK_HEADERS(dirent.h sys/ndir.h sys/dir.h)
AC_CHECK_HEADERS(sys/timeb.h dlfcn.h malloc.h sys/malloc.h)
AC_CHECK_HEADERS(sys/resource.h resource.h sys/syscall.h)
AC_CHECK_FUNCS(getrusage mallinfo syscall)

AC_HEADER_STDC
AC_HEADER_STAT

if test "x$with_asan" = "xyes"; then
   AC_CHECK_LIB(asan,asan_init)
fi

# Time support
AC_CHECK_HEADERS(time.h sys/time.h sys/types.h)
AC_HEADER_TIME
AC_STRUCT_TM
AC_STRUCT_TIMEZONE
AC_EGREP_HEADER(tm_gmtoff,time.h,AC_DEFINE(HAVE_TM_GMTOFF,1),AC_DEFINE(HAVE_TM_GMTOFF,0))
AC_EGREP_HEADER(tm_isdst,time.h,AC_DEFINE(HAVE_TM_ISDST,1),AC_DEFINE(HAVE_TM_ISDST,0))

# Particular functions

AC_CHECK_FUNCS(isatty fstat)
AC_CHECK_FUNCS(strdup strerror_r)
AC_CHECK_FUNCS(access flock fseeko)
AC_CHECK_FUNCS(chown chmod setuid getuid geteuid)
AC_CHECK_FUNCS(getpwnam getpwnam_r getpwgid getpwgid_r)
AC_CHECK_FUNCS(setgid getgid getegid getgroups)
AC_CHECK_FUNCS(getgrnam getgrnam_r getgrgid getgrgid_r)
AC_CHECK_FUNCS(symlink realpath mkdtemp)
AC_CHECK_FUNCS(ftime gettimeofday localtime_r gmtime_r getpid)
AC_CHECK_FUNCS(gethostbyname2_r)
AC_CHECK_FUNCS(getservbyname)
AC_FUNC_STRERROR_R

# Syslog
AC_CHECK_FUNCS(syslog,syslog_target="syslog_libs",syslog_target="")


# Internationalization
AC_CHECK_HEADERS(libintl.h)
AC_CHECK_FUNCS(gettext,[libc_gettext="yes"])
if test "x$libc_gettext" != "xyes"; then
   AC_CHECK_LIB(intl,gettext)
fi
AC_CHECK_FUNCS(gettext,[do_i18n="yes"])
if test "x$do_i18n" = "xyes"; then
   AC_CHECK_FUNCS(textdomain bindtextdomain bindtextdomain_codeset)
fi

AC_ARG_WITH(i18n,--with[out]-i18n built and install message catalogs,,with_i18n="yes")

if test "x$doi18n" = "xyes" && test "x$with_i18n" = "xyes"; then
  I18N="i18n"
  INSTALLI18N="install-i18n"
fi

AC_ARG_WITH(fileinfo,--with[out]-fileinfo record fileinfo,,with_fileinfo="yes")
if test "x$with_fileinfo" = "xyes" ; then
   definefileinfo="-D_FILEINFO=\"\\\"\`./fileinfo ./$< .\`\\\"\""
   versionsh="include/framerd/versions.h"
fi

dnl Use sudo for installations in makefile
AC_ARG_WITH(sudo,--with[out]-sudo use sudo with install,,with_sudo="yes")
if test "x$with_sudo" == "xyes"; then
  AC_CHECK_PROGS(sudo_program,sudo,"na")
  if test "x$sudoprogram" == "xna"; then
    SUDO=""
  else 
    SUDO="unset LD_LIBRARY_PATH; unset DYLD_LIBRARY_PATH; $sudo_program "
  fi
elif test "x$with_sudo" != "xno"; then
  SUDO="$with_sudo "
fi

AC_ARG_WITH(tcmalloc,--with[out]-tcmalloc Use the Google malloc library,,with_tcmalloc="no")
if test $with_tcmalloc = "yes"; then
  AC_CHECK_LIB(tcmalloc,malloc)
fi

AC_CHECK_LIB(dl,dlopen)

AC_ARG_ENABLE(threads,--en/disable-threads Enable multi-threading,,enable_threads=$threads_default)
if test $enable_threads = "yes"; then
  AX_PTHREAD()
  LIBS="$PTHREAD_LIBS $LIBS"
  CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
  AC_DEFINE(U8_THREADS_ENABLED,1)
else
  AC_DEFINE(U8_THREADS_ENABLED,0)
fi

dnl ***********************************
dnl *** Checks for working __thread ***
dnl ***********************************
AC_MSG_CHECKING(for working __thread)
AC_TRY_COMPILE([
            #include <pthread.h>
            __thread int i;
	    static int res1, res2;
	],[
	     void thread_main (void *arg)
	     {
		i = arg;
		sleep (1);
		if (arg == 1) res1 = (i == arg);
		else res2 = (i == arg);
	     }

	     int main () {
		pthread_t t1, t2;
 		i = 5;
		pthread_create (&t1, NULL, thread_main, 1);
		pthread_create (&t2, NULL, thread_main, 2);

		pthread_join (t1, NULL);
		pthread_join (t2, NULL);
		return !(res1 + res2 == 2);
		}
	    ], [
		tls_default=no
		AC_MSG_RESULT(yes)
		AC_DEFINE(HAVE_THREAD_STORAGE_CLASS)],
	       [tls_default=yes
		AC_MSG_RESULT(no)])

dnl This forces the use of the threadlocal storage functions from the pthreads
dnl  library.  Otherwise, the executable will use __thread storage declarations
dnl  when available
AC_ARG_WITH(tls,--with[out]-tls Force use of threadlocal storage,,with_tls=$tls_default)
if test $with_tls = "yes"; then
  AC_DEFINE(U8_FORCE_TLS,1)
else
  AC_DEFINE(U8_FORCE_TLS,0)
fi

AC_CHECK_HEADERS(pthread.h)
AC_CHECK_FUNCS(pthread_rwlock_init pthread_self pthread_create)
AC_CHECK_FUNCS(pthread_mutexattr_settype)

AC_ARG_ENABLE(largefiles,--en/disable-largefiles Enable use of large (> 2GB) file support,,largefiles_enabled=yes)
if test $largefiles_enabled = "yes"; then
  AC_DEFINE(U8_LARGEFILES_ENABLED,1)
else
  AC_DEFINE(U8_LARGEFILES_ENABLED,0)
fi

dnl ***********************************
dnl *** Checks for working constructor attributes ***
dnl ***********************************
AC_MSG_CHECKING(for working __constructor_attributes)
AC_TRY_RUN([static void tmp_meth() __attribute__ ((constructor));
	    static void tmp_meth() { exit(0); }
	    int main(int argc,char **argv) { exit(1); }],
        [AC_DEFINE(HAVE_CONSTRUCTOR_ATTRIBUTES,1) AC_MSG_RESULT(yes)],
	[AC_DEFINE(HAVE_CONSTRUCTOR_ATTRIBUTES,0) AC_MSG_RESULT(no)], 
	[AC_DEFINE(HAVE_CONSTRUCTOR_ATTRIBUTES,1) AC_MSG_RESULT(assume no)])

dnl ***********************************
dnl *** Checks for working __builtin_expect ***
dnl ***********************************
AC_MSG_CHECKING(for working __builtin_expect)
AC_TRY_COMPILE([],[
	     int main (int argc,char *argv[]) {
                if (__builtin_expect((argc>2),1)) return 0;
                else return 1;
		}
	    ], [
		AC_MSG_RESULT(yes)
		AC_DEFINE(HAVE_BUILTIN_EXPECT)],
	       [AC_MSG_RESULT(no)],
	       [AC_MSG_RESULT(assume no)])

# OS X crypto
AC_CHECK_HEADERS(CommonCrypto/CommonDigest.h CommonCrypto/CommonHMAC.h)
AC_CHECK_FUNCS(CCHmacInit)

# OpenSSL crypto
AC_CHECK_HEADERS(openssl/hmac.h)
AC_CHECK_HEADERS(openssl/md5.h)
AC_CHECK_HEADERS(openssl/sha.h)
AC_CHECK_LIB(crypto,EVP_CIPHER_CTX_init)
AC_CHECK_FUNCS(EVP_get_cipherbyname EVP_CIPHER_CTX_init SHA1_Init)

# UUID functions
AC_CHECK_HEADERS(uuid/uuid.h)
AC_CHECK_LIB(uuid,uuid_generate_time)
AC_CHECK_FUNCS(uuid_generate_time)

AC_OUTPUT(makefile)



