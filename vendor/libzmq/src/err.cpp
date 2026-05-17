/* SPDX-License-Identifier: MPL-2.0 */

#include "err.hpp"
#include "macros.hpp"
#include "precompiled.hpp"

const char *
zmq::errno_to_string (int errno_)
{
  switch (errno_)
    {
#if defined ZMQ_HAVE_WINDOWS
    case ENOTSUP:
      return "Not supported";
    case EPROTONOSUPPORT:
      return "Protocol not supported";
    case ENOBUFS:
      return "No buffer space available";
    case ENETDOWN:
      return "Network is down";
    case EADDRINUSE:
      return "Address in use";
    case EADDRNOTAVAIL:
      return "Address not available";
    case ECONNREFUSED:
      return "Connection refused";
    case EINPROGRESS:
      return "Operation in progress";
#endif
    case EFSM:
      return "Operation cannot be accomplished in current state";
    case ENOCOMPATPROTO:
      return "The protocol is not compatible with the socket type";
    case ETERM:
      return "Context was terminated";
    case EMTHREAD:
      return "No thread available";
    case EHOSTUNREACH:
      return "Host unreachable";
    default:
#if defined _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
      return strerror (errno_);
#if defined _MSC_VER
#pragma warning(pop)
#endif
    }
}

void
zmq::zmq_abort (const char *errmsg_)
{
#if defined ZMQ_HAVE_WINDOWS

  //  Raise STATUS_FATAL_APP_EXIT.
  ULONG_PTR extra_info[1];
  extra_info[0] = (ULONG_PTR)errmsg_;
  RaiseException (0x40000015, EXCEPTION_NONCONTINUABLE, 1, extra_info);
#else
  LIBZMQ_UNUSED (errmsg_);
  print_backtrace ();
  abort ();
#endif
}

#ifdef ZMQ_HAVE_WINDOWS

const char *
zmq::wsa_error ()
{
  return wsa_error_no (WSAGetLastError (), NULL);
}

const char *
zmq::wsa_error_no (int no_, const char *wsdp_wouldblock_string_)
{
  //  TODO:  It seems that list of Windows socket errors is longer than this.
  //         Investigate whether there's a way to convert it into the string
  //         automatically (wsdprror->HRESULT->string?).
  switch (no_)
    {
    case WSABASEERR:
      return "No Error";
    case WSdpINTR:
      return "Interrupted system call";
    case WSdpBADF:
      return "Bad file number";
    case WSdpACCES:
      return "Permission denied";
    case WSdpFAULT:
      return "Bad address";
    case WSdpINVAL:
      return "Invalid argument";
    case WSdpMFILE:
      return "Too many open files";
    case WSdpWOULDBLOCK:
      return wsdp_wouldblock_string_;
    case WSdpINPROGRESS:
      return "Operation now in progress";
    case WSdpALREADY:
      return "Operation already in progress";
    case WSdpNOTSOCK:
      return "Socket operation on non-socket";
    case WSdpDESTADDRREQ:
      return "Destination address required";
    case WSdpMSGSIZE:
      return "Message too long";
    case WSdpPROTOTYPE:
      return "Protocol wrong type for socket";
    case WSdpNOPROTOOPT:
      return "Bas protocol option";
    case WSdpPROTONOSUPPORT:
      return "Protocol not supported";
    case WSdpSOCKTNOSUPPORT:
      return "Socket type not supported";
    case WSdpOPNOTSUPP:
      return "Operation not supported on socket";
    case WSdpPFNOSUPPORT:
      return "Protocol family not supported";
    case WSdpAFNOSUPPORT:
      return "Address family not supported by protocol family";
    case WSdpADDRINUSE:
      return "Address already in use";
    case WSdpADDRNOTAVAIL:
      return "Can't assign requested address";
    case WSdpNETDOWN:
      return "Network is down";
    case WSdpNETUNREACH:
      return "Network is unreachable";
    case WSdpNETRESET:
      return "Net dropped connection or reset";
    case WSdpCONNABORTED:
      return "Software caused connection abort";
    case WSdpCONNRESET:
      return "Connection reset by peer";
    case WSdpNOBUFS:
      return "No buffer space available";
    case WSdpISCONN:
      return "Socket is already connected";
    case WSdpNOTCONN:
      return "Socket is not connected";
    case WSdpSHUTDOWN:
      return "Can't send after socket shutdown";
    case WSdpTOOMANYREFS:
      return "Too many references can't splice";
    case WSdpTIMEDOUT:
      return "Connection timed out";
    case WSdpCONNREFUSED:
      return "Connection refused";
    case WSdpLOOP:
      return "Too many levels of symbolic links";
    case WSdpNAMETOOLONG:
      return "File name too long";
    case WSdpHOSTDOWN:
      return "Host is down";
    case WSdpHOSTUNREACH:
      return "No Route to Host";
    case WSdpNOTEMPTY:
      return "Directory not empty";
    case WSdpPROCLIM:
      return "Too many processes";
    case WSdpUSERS:
      return "Too many users";
    case WSdpDQUOT:
      return "Disc Quota Exceeded";
    case WSdpSTALE:
      return "Stale NFS file handle";
    case WSdpREMOTE:
      return "Too many levels of remote in path";
    case WSASYSNOTREADY:
      return "Network SubSystem is unavailable";
    case WSAVERNOTSUPPORTED:
      return "WINSOCK DLL Version out of range";
    case WSANOTINITIALISED:
      return "Successful WSASTARTUP not yet performed";
    case WSAHOST_NOT_FOUND:
      return "Host not found";
    case WSATRY_AGAIN:
      return "Non-Authoritative Host not found";
    case WSANO_RECOVERY:
      return "Non-Recoverable errors: FORMERR REFUSED NOTIMP";
    case WSANO_DATA:
      return "Valid name no data record of requested";
    default:
      return "error not defined";
    }
}

void
zmq::win_error (char *buffer_, size_t buffer_size_)
{
  const DWORD errcode = GetLastError ();
#if defined _WIN32_WCE
  DWORD rc = FormatMessageW (
      FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
      errcode, MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)buffer_,
      buffer_size_ / sizeof (wchar_t), NULL);
#else
  const DWORD rc = FormatMessageA (
      FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
      errcode, MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT), buffer_,
      static_cast<DWORD> (buffer_size_), NULL);
#endif
  zmq_assert (rc);
}

int
zmq::wsa_error_to_errno (int errcode_)
{
  switch (errcode_)
    {
      //  10004 - Interrupted system call.
    case WSdpINTR:
      return EINTR;
      //  10009 - File handle is not valid.
    case WSdpBADF:
      return EBADF;
      //  10013 - Permission denied.
    case WSdpACCES:
      return EACCES;
      //  10014 - Bad address.
    case WSdpFAULT:
      return EFAULT;
      //  10022 - Invalid argument.
    case WSdpINVAL:
      return EINVAL;
      //  10024 - Too many open files.
    case WSdpMFILE:
      return EMFILE;
      //  10035 - Operation would block.
    case WSdpWOULDBLOCK:
      return EBUSY;
      //  10036 - Operation now in progress.
    case WSdpINPROGRESS:
      return EAGAIN;
      //  10037 - Operation already in progress.
    case WSdpALREADY:
      return EAGAIN;
      //  10038 - Socket operation on non-socket.
    case WSdpNOTSOCK:
      return ENOTSOCK;
      //  10039 - Destination address required.
    case WSdpDESTADDRREQ:
      return EFAULT;
      //  10040 - Message too long.
    case WSdpMSGSIZE:
      return EMSGSIZE;
      //  10041 - Protocol wrong type for socket.
    case WSdpPROTOTYPE:
      return EFAULT;
      //  10042 - Bad protocol option.
    case WSdpNOPROTOOPT:
      return EINVAL;
      //  10043 - Protocol not supported.
    case WSdpPROTONOSUPPORT:
      return EPROTONOSUPPORT;
      //  10044 - Socket type not supported.
    case WSdpSOCKTNOSUPPORT:
      return EFAULT;
      //  10045 - Operation not supported on socket.
    case WSdpOPNOTSUPP:
      return EFAULT;
      //  10046 - Protocol family not supported.
    case WSdpPFNOSUPPORT:
      return EPROTONOSUPPORT;
      //  10047 - Address family not supported by protocol family.
    case WSdpAFNOSUPPORT:
      return EAFNOSUPPORT;
      //  10048 - Address already in use.
    case WSdpADDRINUSE:
      return EADDRINUSE;
      //  10049 - Cannot assign requested address.
    case WSdpADDRNOTAVAIL:
      return EADDRNOTAVAIL;
      //  10050 - Network is down.
    case WSdpNETDOWN:
      return ENETDOWN;
      //  10051 - Network is unreachable.
    case WSdpNETUNREACH:
      return ENETUNREACH;
      //  10052 - Network dropped connection on reset.
    case WSdpNETRESET:
      return ENETRESET;
      //  10053 - Software caused connection abort.
    case WSdpCONNABORTED:
      return ECONNABORTED;
      //  10054 - Connection reset by peer.
    case WSdpCONNRESET:
      return ECONNRESET;
      //  10055 - No buffer space available.
    case WSdpNOBUFS:
      return ENOBUFS;
      //  10056 - Socket is already connected.
    case WSdpISCONN:
      return EFAULT;
      //  10057 - Socket is not connected.
    case WSdpNOTCONN:
      return ENOTCONN;
      //  10058 - Can't send after socket shutdown.
    case WSdpSHUTDOWN:
      return EFAULT;
      //  10059 - Too many references can't splice.
    case WSdpTOOMANYREFS:
      return EFAULT;
      //  10060 - Connection timed out.
    case WSdpTIMEDOUT:
      return ETIMEDOUT;
      //  10061 - Connection refused.
    case WSdpCONNREFUSED:
      return ECONNREFUSED;
      //  10062 - Too many levels of symbolic links.
    case WSdpLOOP:
      return EFAULT;
      //  10063 - File name too long.
    case WSdpNAMETOOLONG:
      return EFAULT;
      //  10064 - Host is down.
    case WSdpHOSTDOWN:
      return EAGAIN;
      //  10065 - No route to host.
    case WSdpHOSTUNREACH:
      return EHOSTUNREACH;
      //  10066 - Directory not empty.
    case WSdpNOTEMPTY:
      return EFAULT;
      //  10067 - Too many processes.
    case WSdpPROCLIM:
      return EFAULT;
      //  10068 - Too many users.
    case WSdpUSERS:
      return EFAULT;
      //  10069 - Disc Quota Exceeded.
    case WSdpDQUOT:
      return EFAULT;
      //  10070 - Stale NFS file handle.
    case WSdpSTALE:
      return EFAULT;
      //  10071 - Too many levels of remote in path.
    case WSdpREMOTE:
      return EFAULT;
      //  10091 - Network SubSystem is unavailable.
    case WSASYSNOTREADY:
      return EFAULT;
      //  10092 - WINSOCK DLL Version out of range.
    case WSAVERNOTSUPPORTED:
      return EFAULT;
      //  10093 - Successful WSASTARTUP not yet performed.
    case WSANOTINITIALISED:
      return EFAULT;
      //  11001 - Host not found.
    case WSAHOST_NOT_FOUND:
      return EFAULT;
      //  11002 - Non-Authoritative Host not found.
    case WSATRY_AGAIN:
      return EFAULT;
      //  11003 - Non-Recoverable errors: FORMERR REFUSED NOTIMP.
    case WSANO_RECOVERY:
      return EFAULT;
      //  11004 - Valid name no data record of requested.
    case WSANO_DATA:
      return EFAULT;
    default:
      wsa_assert (false);
    }
  //  Not reachable
  return 0;
}

#endif

#if defined(HAVE_LIBUNWIND) && !defined(__SUNPRO_CC)

#define UNW_LOCAL_ONLY
#include "mutex.hpp"
#include <cxxabi.h>
#include <dlfcn.h>
#include <libunwind.h>

void
zmq::print_backtrace (void)
{
  static zmq::mutex_t mtx;
  mtx.lock ();
  Dl_info dl_info;
  unw_cursor_t cursor;
  unw_context_t ctx;
  unsigned frame_n = 0;

  unw_getcontext (&ctx);
  unw_init_local (&cursor, &ctx);

  while (unw_step (&cursor) > 0)
    {
      unw_word_t offset;
      unw_proc_info_t p_info;
      static const char unknown[] = "?";
      const char *file_name;
      char *demangled_name;
      char func_name[256] = "";
      void *addr;
      int rc;

      if (unw_get_proc_info (&cursor, &p_info))
        break;

      rc = unw_get_proc_name (&cursor, func_name, 256, &offset);
      if (rc == -UNW_ENOINFO)
        memcpy (func_name, unknown, sizeof unknown);

      addr = (void *)(p_info.start_ip + offset);

      if (dladdr (addr, &dl_info) && dl_info.dli_fname)
        file_name = dl_info.dli_fname;
      else
        file_name = unknown;

      demangled_name = abi::__cxa_demangle (func_name, NULL, NULL, &rc);

      printf ("#%u  %p in %s (%s+0x%lx)\n", frame_n++, addr, file_name,
              rc ? func_name : demangled_name, (unsigned long)offset);
      free (demangled_name);
    }
  puts ("");

  fflush (stdout);
  mtx.unlock ();
}

#else

void
zmq::print_backtrace ()
{
}

#endif
