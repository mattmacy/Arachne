# This script provides a convenience command ta (thread-arachne) to switch to
# an Arachne thread, given a ThreadContext* as an argument.

import gdb


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

class ThreadArachneCommand (gdb.Command):
  "Thread switch command for user threads in Arachne threading library."

  def __init__ (self):
    super (ThreadArachneCommand, self).__init__ ("thread-arachne",
                         gdb.COMMAND_STACK,
                         gdb.COMPLETE_SYMBOL, True)
    self.kernelThreadMap = {}
    self.maxFibers = gdb.parse_and_eval("Arachne::maxThreadsPerCore")
    gdb.execute("alias -a ta = thread-arachne", True)


  def restoreKernelThread(self, from_tty):
    kThreadNum = gdb.selected_thread().num
    if not kThreadNum in self.kernelThreadMap:
      print(f"{kThreadNum} not in kernelThreadMap")
      return
    gdb.execute("set ($rsp)={0}".format(self.kernelThreadMap[kThreadNum][0]), from_tty)
    gdb.execute("set ($pc)={0}".format(self.kernelThreadMap[kThreadNum][1]), from_tty)

    # Remove it so that we will save it again when we leave this context
    del self.kernelThreadMap[kThreadNum]

  # Switch to a user thread, but do not change Arachne::core.loadedContext, since we
  # use that to determine whether we need to load a saved context from the
  # kernelThreadMap.
  def thread(self, threadContext, from_tty):
    # Check if we are switching to the currently active context
    loadedContext = gdb.parse_and_eval("Arachne::core.loadedContext")
    if isinstance(threadContext, str):
      threadContext = gdb.parse_and_eval(threadContext)
    if int(loadedContext) == int(threadContext):
      self.restoreKernelThread(from_tty)
      return

    originalSP = gdb.parse_and_eval("$sp")
    originalPC = int(gdb.parse_and_eval("$pc"))
    gdb.execute("set $__thread_context = ((Arachne::ThreadContext*){0})".format(threadContext))
    gdb.execute("set $rsp = $__thread_context->sp + Arachne::SPACE_FOR_SAVED_REGISTERS", from_tty)
    gdb.execute("set $pc = arachne_context_pushed", from_tty)
    kThreadNum = gdb.selected_thread().num
    if not kThreadNum in self.kernelThreadMap:
        self.kernelThreadMap[kThreadNum] = (originalSP, originalPC)

  def invoke(self, arg, from_tty):
    pass
    arg = arg.strip()
    if arg == "":
        # Restore to the current kernel thread if it is available.
        self.restoreKernelThread(from_tty)
        return
    # Check if we were passed a fiber id instead
    if arg.isnumeric() and int(arg) < 256*self.maxFibers:
      context, runningContext = thread_to_context(int(arg))
      if not context:
        print(f"no fiber found for {arg}")
        return
      loadedContext = gdb.parse_and_eval("Arachne::core.loadedContext")
      if context == loadedContext:
        return
      if context != runningContext:
        self.thread(context, from_tty)
        return
      threads = gdb.inferiors()[0].threads()
      for thread in threads:
        thread.switch()
        loadedContext = gdb.parse_and_eval("Arachne::core.loadedContext")
        if loadedContext == context:
          break
    # Verify that the type is correct. Maybe eventually accept an index instead.
    threadContext = gdb.parse_and_eval(arg)
    typestring = str(threadContext.type)
    if typestring.strip() != "Arachne::ThreadContext *":
      print("Please pass an Arachne::ThreadContext*")
      return

    # Check if the provided threadcontext is NULL, and do nothing if it is.
    threadContextValue = int(threadContext)
    if threadContextValue == 0:
        print("A NULL pointer was passed!")
        return

    self.thread(arg, from_tty)

ThreadArachneCommand()
