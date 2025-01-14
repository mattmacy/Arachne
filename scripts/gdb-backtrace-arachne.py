# This script provides convenience command bta (backtrace-arachne) to backtrace
# an Arachne thread, given a ThreadContext* as an argument.

from __future__ import print_function

import gdb

def is_arachne_thread(coreId, maskAndCountPointer):
  return (coreId >= 0) and (maskAndCountPointer != 0)

def thread_to_context(tid):
  maxFibers = gdb.parse_and_eval("Arachne::maxThreadsPerCore")
  coreThreadId = tid % maxFibers
  coreId = tid // int(maxFibers)
  coreMapId = gdb.parse_and_eval(f"Arachne::coreMap._M_impl._M_start+{coreId}")
  coreMapEnd = gdb.parse_and_eval(f"Arachne::coreMap._M_impl._M_finish")
  if coreMapId > coreMapEnd:
    print(f"tid: {tid} -> coreId: {coreId} is out of range")
    return None, None
  corePtr = coreMapId.dereference()
  maskAndCountPointer = corePtr.dereference()['localOccupiedAndCount']
  runningContext = corePtr.dereference()['loadedContext']
  if maskAndCountPointer == 0:
    return None, None
  bitmask = maskAndCountPointer.dereference()['_M_i']['occupied']
  if not ((bitmask >> coreThreadId) & 1):
    return None, None
  coreThreadContextId= gdb.parse_and_eval(f"(Arachne::coreMap._M_impl._M_start+{coreId}).localThreadContexts[{coreThreadId}]")
  return coreThreadContextId, runningContext

class BackTraceArachneCommand (gdb.Command):
  "Backtrace command for user threads in Arachne threading library."

  def __init__ (self):
    super (BackTraceArachneCommand, self).__init__ ("backtrace-arachne",
                         gdb.COMMAND_STACK,
                         gdb.COMPLETE_SYMBOL, True)
    gdb.execute("alias -a bta = backtrace-arachne", True)

  def inner_bt(self, from_tty):
    try:
      frame = gdb.newest_frame()
      idx = 0
      while frame and frame.pc():
        framefn = frame.function()
        framepc = frame.pc()
        sal = frame.find_sal()
        if sal and sal.line != 0:
          framefile = sal.symtab.filename
          frameline = sal.line
          print(f"#{idx} {framepc:#x} {framefn} at {framefile}:{frameline}")
          idx = idx + 1
        elif framefn:
          print(f"#{idx} {framepc:#x} {framefn}")
          idx = idx + 1
        else:
          print(f"#{idx} {framepc:#x}")
        frame = frame.older()
    except:
      pass

  def backtrace_all(self, from_tty):
    maxFibers = gdb.parse_and_eval("Arachne::maxThreadsPerCore")
    threads = gdb.inferiors()[0].threads()
    curthread = gdb.selected_thread()
    for thread in threads:
      thread.switch()
      coreId = gdb.parse_and_eval("Arachne::core.id")
      maskAndCountPointer = gdb.parse_and_eval("Arachne::core.localOccupiedAndCount")
      if not is_arachne_thread(coreId, maskAndCountPointer):
        continue
      self.backtrace_local(from_tty)
    curthread.switch()

  def backtrace_local(self, from_tty):
    maxFibers = gdb.parse_and_eval("Arachne::maxThreadsPerCore")
    # Backtrace all threadcontexts that are occupied in the current core
    coreId = gdb.parse_and_eval("Arachne::core.id")
    maskAndCountPointer = gdb.parse_and_eval("Arachne::core.localOccupiedAndCount")
    if not is_arachne_thread(coreId, maskAndCountPointer):
      print("Current core is not an Arachne core!")
      return
    bitmask = maskAndCountPointer.dereference()['_M_i']['occupied']
    # Perform a backtrace on all the occupied bits.
    for i in range(maxFibers):
      if (bitmask >> i) & 1:
        threadContext = gdb.parse_and_eval("Arachne::core.localThreadContexts[{0}]".format(i))
        tid = coreId*maxFibers + i
        print("Arachne Thread #{0} [{1}]:{2}: {3}".format(tid, coreId, i, threadContext))
        try:
          self.backtrace(threadContext, from_tty)
        except:
          pass

  def backtrace(self, threadContext, from_tty):
    # Check if we are backtracing the current context
    loadedContext = gdb.parse_and_eval("Arachne::core.loadedContext")
    if isinstance(threadContext, str):
      threadContext = gdb.parse_and_eval(threadContext)
    if int(loadedContext) == int(threadContext):
      #gdb.execute("backtrace", from_tty)
      self.inner_bt(from_tty)
      return

    SP = gdb.parse_and_eval("$sp")
    PC = int(gdb.parse_and_eval("$pc"))

    gdb.execute("set $__thread_context = ((Arachne::ThreadContext*){0})".format(threadContext))
    gdb.execute("set $rsp = $__thread_context->sp + Arachne::SPACE_FOR_SAVED_REGISTERS", from_tty)
    gdb.execute("set $pc = arachne_context_pushed", from_tty)
    self.inner_bt(from_tty)

    # Restore
    gdb.execute("set  $rsp = {0}".format(SP), from_tty)
    gdb.execute("set  $pc = {0}".format(PC), from_tty)

  def invoke(self, arg, from_tty):
    arg = arg.strip()
    maxFibers = gdb.parse_and_eval("Arachne::maxThreadsPerCore")
    if arg == "":
      self.backtrace_local(from_tty)
      return
    if arg == "all":
      self.backtrace_all(from_tty)
      return
    # Check if we were passed a fiber id instead
    if arg.isnumeric() and int(arg) < 256*maxFibers:
      context, runningContext = thread_to_context(int(arg))
      if not context:
        print(f"no fiber found for {arg}")
        return
      loadedContext = gdb.parse_and_eval("Arachne::core.loadedContext")
      curthread = None
      if context == runningContext and loadedContext != runningContext:
        curthread = gdb.selected_thread()
        threads = gdb.inferiors()[0].threads()
        for thread in threads:
          thread.switch()
          loadedContext = gdb.parse_and_eval("Arachne::core.loadedContext")
          if loadedContext == context:
            break
      self.backtrace(context, from_tty)
      curthread.switch()
      return

    # Verify that the type is correct
    typestring=str(gdb.parse_and_eval(arg).type)
    if typestring.strip() != "Arachne::ThreadContext *":
      print(f"Please pass an Arachne::ThreadContext* not {typestring} for {arg}")
      return

    # Check if the provided threadcontext is NULL, and do nothing if it is.
    threadcontextvalue = int(gdb.parse_and_eval(arg))
    if threadcontextvalue == 0:
        print("A NULL pointer was passed!")
        return

    self.backtrace(arg, from_tty)

BackTraceArachneCommand()
