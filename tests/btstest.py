#!/usr/bin/env python3

"""
Grab input log.
"""

import argparse
import contextlib
import decimal
import io
import json
import logging
import os
import re
import shutil
import socket
import subprocess
import sys
import time
import urllib.parse
import urllib.request

@contextlib.contextmanager
def add_to_sys_path(path_additions):
    old_path = sys.path[:]
    new_path = sys.path + path_additions
    sys.path = new_path
    yield
    sys.path = old_path
    return

class ParseError(Exception):
    pass

class RPCError(Exception):
    def __init__(self, http_error_code=-1, error_data=None):
        self.http_error_code = http_error_code
        self.error_data = error_data
        return

class RPCClient(object):
    def __init__(self, credentials):

        self.password_mgr = urllib.request.HTTPPasswordMgrWithDefaultRealm()

        self.rpc_url = "http://{host}:{port}/rpc".format(**credentials)
        self.password_mgr.add_password(None, self.rpc_url, credentials["rpc_user"], credentials["rpc_password"])

        self.basic_auth_handler = urllib.request.HTTPBasicAuthHandler(self.password_mgr)

        # create "opener" (OpenerDirector instance)
        self.url_opener = urllib.request.build_opener(self.basic_auth_handler)

        self.next_request_id = 1

        return

    def call(self, method, *args):
        logging.debug("calling URL: "+str(self.rpc_url))
        # use the opener to fetch a URL
        o = { "method" : method, "params" : args, "id" : self.get_next_id() }
        logging.debug("data: "+str(o))
        #post_data = urllib.parse.urlencode(o).encode("UTF-8")
        post_data = json.dumps(o).encode("UTF-8")
        req = urllib.request.Request(self.rpc_url, post_data)
        try:
            response = self.url_opener.open(req)
            response_content = response.read().decode("UTF-8")
            d = json.loads(response_content, parse_float=decimal.Decimal)
            logging.debug("result: "+str(d))
            result = d["result"]
            logging.debug("result: "+str(result))
        except urllib.error.HTTPError as e:
            response_content = e.read().decode("UTF-8")
            d = json.loads(response_content, parse_float=decimal.Decimal)
            logging.debug("error result: "+str(d))
            raise RPCError(http_error_code=e.code, error_data=d)
        return result

    def get_next_request_id(self):
        result = self.next_request_id
        self.next_request_id = result+1
        return result

    def get_next_id(self):
        result = self.next_request_id
        self.next_request_id = result+1
        return result

    def __call__(self, method, *args):
        return self.call(method, args)

    def wait_for_rpc(self):
        #
        # retry connecting until we succeed or timeout is reached
        #

        retry_loop_start = time.time()

        while True:
            try:
                self("get_info")
            except Exception as e:
                # TODO: exception type
                logging.debug("can't connect:")
                logging.debug(e)
                now = time.time()
                dt = now - retry_loop_start
                if dt < 10:
                    time.sleep(0.25)
                elif dt < 20:
                    time.sleep(1)
                elif dt < 60:
                    time.sleep(10)
                elif dt < 60*5:
                    time.sleep(15)
                else:
                    logging.debug("timed out")
                    # TODO : exception type
                    raise RuntimeError()
                continue
            break

        dt = time.time() - retry_loop_start
        logging.info("succeeded connecting to HTTP RPC after {} seconds".format(dt))
        return

class PortAssigner(object):
    def __init__(self, min_port=30000, max_port=40000):
        self.min_port = min_port
        self.max_port = max_port
        self.next_port = min_port
        return

    def __call__(self):
        result = self.next_port
        self.next_port = self.next_port+1
        if self.next_port >= self.max_port:
            self.next_port = self.min_port
        return self.next_port

class LinuxPortAssigner(PortAssigner):
    #
    # this class queries the Linux kernel for ports in use
    # and avoids those that are currently in use
    #
    # it works on any system that provides /proc/net/tcp in Linux format
    #
    # if the file does not exist, it doesn't check ports are in use,
    # so this class should still be safe to use on non-Linux OS
    #

    def __init__(self, min_port=30000, max_port=40000):
        PortAssigner.__init__(self, min_port, max_port)
        return

    def __call__(self):
        if not os.path.exists("/proc/net/tcp"):
            return PortAssigner.__call__(self)
        ports_in_use = set()
        with open("/proc/net/tcp", "r") as f:
            for line in f:
                u = line.split()
                if len(u) < 3:
                    continue
                if not u[2].endswith(":0000"):
                    continue
                port_in_use = u[1][-5:]
                if not port_in_use.startswith(":"):
                    continue
                portnum = int(port_in_use[1:], 16)
                ports_in_use.add(portnum)
        while True:
            port = PortAssigner.__call__(self)
            if port not in ports_in_use:
                break
        return port

class ClientProcess(object):

    default_client_exe = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "programs", "client", "bitsharestestnet_client"))
    default_rpc_port = LinuxPortAssigner()
    default_http_port = default_rpc_port
    default_p2p_port = default_rpc_port
    default_username = "username"
    default_password = "password"
    default_genesis_config = os.path.join(os.path.dirname(__file__), "drltc_tests", "genesis.json")

    def __init__(self,
        name="default",
        testname=None,
        client_exe=None,
        p2p_port=None,
        rpc_port=None,
        http_port=None,
        username=None,
        password=None,
        genesis_config=None,
        testdir=None,
        rpc_client=None,
        debug_stop=False,
        xts="XTS",
        ):
        self.name = name

        if testname is None:
            raise RuntimeError("Must set testname when creating ClientProcess")
        self.testname = testname

        if client_exe is None:
            client_exe = self.default_client_exe
        self.client_exe = client_exe

        if p2p_port is None:
            p2p_port = self.default_p2p_port
        self.p2p_port = p2p_port

        if rpc_port is None:
            rpc_port = self.default_rpc_port
        self.rpc_port = rpc_port

        if http_port is None:
            http_port = self.default_http_port
        self.http_port = http_port

        if username is None:
            username = self.default_username
        self.username = username

        if password is None:
            password = self.default_password
        self.password = password

        if genesis_config is None:
            genesis_config = self.default_genesis_config
        self.genesis_config = genesis_config

        if testdir is None:
            testdir = self.get_default_testdir()
        self.testdir = testdir

        self.process_object = None

        self.stdout_file = None
        self.stderr_file = None

        self.rpc_client = rpc_client

        self.debug_stop = debug_stop

        self.xts = xts

        return

    def __enter__(self):
        self.start()
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.stop()
        return

    def get_default_testdir(self):
        return os.path.join(
            os.path.dirname(__file__),
            "btstests",
            "out",
            self.testname,
            )

    def start(self):
        # execute process object and wait for RPC to become available
        if self.process_object is not None:
            # TODO: exception type
            raise RuntimeError("start() called multiple times")

        data_dir = os.path.abspath(os.path.join(self.testdir, self.name))
        if not os.path.exists(self.testdir):
            os.makedirs(self.testdir)
        if os.path.exists(data_dir):
            shutil.rmtree(data_dir)
        os.makedirs(data_dir)

        if callable(self.p2p_port):
            self.p2p_port = self.p2p_port()

        if callable(self.rpc_port):
            self.rpc_port = self.rpc_port()

        if callable(self.http_port):
            self.http_port = self.http_port()

        genesis_file = os.path.join(data_dir, "genesis.json")
        genesis_config = self.genesis_config

        if not isinstance(genesis_config, dict):
            with open(self.genesis_config, "r") as f:
                genesis_config = json.load(f)

        # write genesis to output file after trimming keys
        for delegate in genesis_config["delegates"]:
            if (
                delegate["owner"].startswith("XTS") or
                delegate["owner"].startswith("DVS") or
                delegate["owner"].startswith("BTS")
               ):
                delegate["owner"] = self.xts+delegate["owner"][3:]

        # copy genesis config to file
        with open(genesis_file, "w") as f:
            json.dump(genesis_config, f, indent=4, sort_keys=True)

        with open(os.path.join(data_dir, "checkpoints.json"), "w") as f:
            #
            # Since the client uses builtin checkpoints beyond the
            # last checkpoint in the file, an empty checkpoint file
            # won't work.  Use a hack to workaround this:  Set a
            # bogus checkpoint very far in the future.
            #
            f.write("[[999999999, \"9999999999999999999999999999999999999999\"]]\n")

        args = [
            "--p2p-port", str(self.p2p_port),
            "--rpcuser", self.username,
            "--rpcpassword", self.password,
            "--rpcport", str(self.rpc_port),
            "--httpport", str(self.http_port),
            "--disable-default-peers",
            "--disable-peer-advertising",
            "--min-delegate-connection-count", "0",
            "--upnp", "0",
            "--genesis-config", genesis_file,
            "--data-dir", data_dir,
            "--server",
            #"--log-commands", os.path.join(data_dir, "console.log"),
            ]

        if self.debug_stop:
            args.append("--debug-stop")

        logging.debug("args: "+str(args))

        self.stdout_file = open(os.path.join(data_dir, "stdout.txt"), "wb")
        self.stderr_file = open(os.path.join(data_dir, "stderr.txt"), "wb")

        self.process_object = subprocess.Popen(
            [self.client_exe]+args,
            stdout=self.stdout_file,
            stderr=self.stderr_file,
            stdin=subprocess.PIPE,
            cwd=data_dir,
            )

        with open(self.get_pid_filename(), "w") as pidfile:
            pidfile.write(str(self.process_object.pid)+"\n")

        return

    def get_pid_filename(self):
        return os.path.join(self.testdir, self.name + ".pid")

    def stop(self):
        if self.process_object is None:
            return
        if self.process_object.poll() is not None:
            # subprocess already exited, no cleanup is necessary
            return

        sys.stderr.write("\nSIGTERM")
        sys.stderr.flush()
        # send SIGTERM, then SIGKILL if we don't exit quickly (or cross-platform equivalents)
        self.process_object.terminate()
        for dot in range(12):
            try:
                sys.stderr.write(".")
                sys.stderr.flush()
                self.process_object.wait(timeout=0.25)
            except subprocess.TimeoutExpired:
                continue
            break
        else:
            sys.stderr.write("\nSIGKILL")
            sys.stderr.flush()
            self.process_object.kill()
            for dot in range(12):
                try:
                    sys.stderr.write(".")
                    sys.stderr.flush()
                    self.process_object.wait(timeout=0.25)
                except subprocess.TimeoutExpired:
                    continue
                break
        sys.stderr.write("\n")
        sys.stderr.flush()

        if self.stdout_file is not None:
            self.stdout_file.close()
        if self.stderr_file is not None:
            self.stderr_file.close()

        try:
            os.remove(self.get_pid_filename())
        except OSError as e:
            pass

        return

class TestClient(object):
    def __init__(self, name="", rpc_client=None):
        self.last_command_output = ""
        self.last_command_pos = 0       # how far we've scanned the output
        self.last_command_failed = False
        self.name = name
        self.rpc_client = rpc_client
        self.re_cache = {}
        self.failure_count = 0
        return

    def expect_fail(self, expected_value, fail_callback=None):
        m = self.last_command_pos
        n = m
        while ((n < len(self.last_command_output)) and
               (self.last_command_output[n] not in " \t\f\v\r\n")):
            n += 1
        got_value = self.last_command_output[m:n]
        if not self.last_command_failed:
            self.last_command_failed = True
            self.failure_count += 1
        self.last_command_pos = n
        if fail_callback is not None:
            fail_callback(expected_value, got_value)
        return

    def skip_whitespace(self):
        m = self.last_command_pos
        while ((m < len(self.last_command_output)) and
               (self.last_command_output[m] in " \t\f\v\r\n")):
            m += 1
        self.last_command_pos = m
        return

    def expect_str(self, s, match_callback=None, fail_callback=None):
        # forward past any whitespace
        self.skip_whitespace()
        m = self.last_command_pos
        n = m+len(s)
        if self.last_command_output[m:n] == s:
            logging.debug("set last_command_pos = {}".format(n))
            self.last_command_pos = n
            if match_callback is not None:
                match_callback(self.last_command_output[m:n])
        else:
            # try to resume match by skipping non-whitespace characters
            self.expect_fail(s, fail_callback)
        return

    def expect_regex(self, regex, match_callback=None, fail_callback=None):
        logging.debug("expecting regex: {}".format(regex))
        logging.debug("matching at {} against {}".format(self.last_command_pos, repr(self.last_command_output)))
        logging.debug("match str: "+repr(self.last_command_output[self.last_command_pos:]))
        self.skip_whitespace()
        compiled_re = self.re_cache.get(regex)
        if compiled_re is None:
            compiled_re = re.compile(regex)
            self.re_cache[regex] = compiled_re

        m = compiled_re.match(self.last_command_output, self.last_command_pos)
        if m is None:
            self.expect_fail(regex, fail_callback)
            return None
        
        self.last_command_pos = m.end()
        logging.debug("set last_command_pos = {}".format(self.last_command_pos))

        if match_callback is not None:
            match_callback(self.last_command_output[m.start():m.end()])
        return m.groupdict()

    def expect_json(self, match_callback=None, fail_callback=None):
        self.skip_whitespace()
        # inefficient but we have no choice
        m = self.last_command_pos
        stream = io.StringIO(self.last_command_output[m:])
        try:
            json_object = json.load(stream)
        except ValueError:
            self.expect_fail("<json>", fail_callback)
            return
        n = m+stream.tell()
        self.last_command_pos = n
        if match_callback is not None:
            match_callback(self.last_command_output[m:n])
        return

    def expect_reltime(self, match_callback=None, fail_callback=None):
        return self.expect_regex( r"[0-9]+\s+(?:second|minute|hour|day|week|month|year)[s]?\s+(?:ago|old|remaining|in\s+the\s+future)",
            match_callback=match_callback, fail_callback=fail_callback )

    def expect_isotime(self, match_callback=None, fail_callback=None):
        return self.expect_regex( r"[0-9]+[-][0-9]{2}[-][0-9]{2}[T][0-9]{2}[:][0-9]{2}[:][0-9]{2}",
            match_callback=match_callback, fail_callback=fail_callback )

    def reset_last_command(self, new_command_output=None):
        self.last_command_output = new_command_output
        self.last_command_pos = 0
        logging.debug("set last_command_pos = {}".format(self.last_command_pos))
        self.last_command_failed = False
        return

    def execute_cmd(self, cmd, expect_enabled=True, extra_output_callback=None, echo=False):
        # store result
        if echo:
            print(">>> "+cmd)
        result = self.rpc_client.call("execute_command_line", cmd)
        self.reset_last_command(result)
        return

    def finish_cmd(self):
        m = self.last_command_pos
        return self.last_command_output[self.last_command_pos:]

class Token(object):
    def __init__(self, text="", lineno=-1, col=-1):
        self.text = text
        self.lineno = lineno
        self.col = col
        return

def _token_iterator(regex, s):
    """
    Returns match for regex, or non-matching characters
    """
    i = 0
    for m in regex.finditer(s):
        j = m.start()
        if i < j:
            yield s[i:j]
        i = m.end()
        yield m.group(0)
    return

re_nl = re.compile(r"\r\n?|\n")

def _line_start_iter(s):
    yield 0
    for m in re_nl.finditer(s):
        yield m.end()
    return

def token_iterator(regex, s):
    """
    Returns match for regex, or non-matching characters
    """
    ls_iter = _line_start_iter(s)
    ls_index = -1
    
    lineno = 0
    col = 1
    
    char_offset = 0
    for t in _token_iterator(regex, s):
        while char_offset >= ls_index:
            lineno += 1
            try:
                ls_index = next(ls_iter)
            except StopIteration:
                ls_index = len(s)+1
        col = char_offset-ls_index+1
        yield Token(t, lineno, col)
        char_offset += len(t)
    return

class Test(object):
    def __init__(self):
        self.context = {}
        self.name2client = {}
        self.context["active_client"] = ""
        self.context["arg"] = self.append_arg
        self.context["expect_str"] = self.expect_str
        self.context["expect_regex"] = self.expect_regex
        self.context["expect_json"] = self.expect_json
        self.context["expect_reltime"] = self.expect_reltime
        self.context["expect_isotime"] = self.expect_isotime
        self.context["run_testdir"] = self.run_testdir
        self.context["regex"] = self.expect_regex
        self.context["register_client"] = self.register_client
        self.context["expect_enabled"] = True
        self.context["showmatch_enabled"] = False
        self.context["showlineno_enabled"] = False
        self.context["current_test"] = self
        self.context["matchbuf"] = []
        self.context["_btstest"] = sys.modules[__name__]
        self.last_command_client = None
        return

    def load_testenv(self, testenv_filename):
        testenv_filename = os.path.abspath(testenv_filename)
        with open(testenv_filename, "r") as f:
            testenv_script = f.read()
        compiled_testenv = compile(testenv_script, testenv_filename, "exec", dont_inherit=1)

        self.context["my_filename"] = testenv_filename
        self.context["my_path"] = os.path.dirname(testenv_filename)
        self.context["testname"] = os.path.basename(self.context["my_path"])

        with add_to_sys_path([os.path.dirname(testenv_filename)]):
            exec(compiled_testenv, self.context)
        return

    def interpret_expr(self, expr, filename=""):
        expr = expr.strip()
        compiled_expr = compile(expr, filename, "exec")
        result = exec(compiled_expr, self.context)
        if isinstance(result, str):
            self.expect_str(result, match_callback=self.on_match)
        return

    def get_active_client(self):
        return self.name2client.get(self.context.get("active_client"))

    def on_fail_literal_str(self, expected_value, got_value):
        # TODO: respect showmatch_enabled here
        self.print_output("!{ " + expected_value + " }!~{ " + got_value + " }~")
        return

    def on_pass_literal_str(self, s):
        self.print_output(s)
        return

    def append_arg(self, arg):
        s_arg = str(arg)
        cmd_text.append(s_arg)
        self.matchbuf.append(s_arg)
        return

    def expect_literal_str(self, data):
        return self._expect_str(data,
            match_callback=self.on_pass_literal_str,
            fail_callback=self.on_fail_literal_str)

    def expect_str(self, data, match_callback=None):
        self._expect_str(data, match_callback)
        return

    def _expect_str(self, data, match_callback=None, fail_callback=None):
        if not self.context["expect_enabled"]:
            return True
        if data == "":
            return True
        client = self.get_active_client()
        client.expect_str(data,
            match_callback=match_callback,
            fail_callback=fail_callback)
        return

    def expect_regex(self, regex, match_callback=None):
        if not self.context["expect_enabled"]:
            return
        client = self.get_active_client()
        client.expect_regex(regex, match_callback=self.on_match)
        # TODO:  write regex result
        return

    def expect_json(self):
        if not self.context["expect_enabled"]:
            return
        client = self.get_active_client()
        client.expect_json(match_callback=self.on_match)
        return

    def expect_reltime(self):
        if not self.context["expect_enabled"]:
            return
        client = self.get_active_client()
        client.expect_reltime(match_callback=self.on_match)
        return

    def expect_isotime(self):
        if not self.context["expect_enabled"]:
            return
        client = self.get_active_client()
        client.expect_isotime(match_callback=self.on_match)
        return

    def on_match(self, s):
        self.context["matchbuf"].append(s)
        return

    def dump_matchbuf(self, do_print=False):
        if do_print:
            self.print_output("~{"+"".join(self.context["matchbuf"])+"}~")
        del self.context["matchbuf"][:]
        return

    def parse_metacommand(self, cmd):
        cmd = cmd.split()
        if cmd[0] == "!client":
            self.context["active_client"] = cmd[1]
            return
        elif cmd[0] == "!expect":
            if cmd[1] == "enable":
                self.context["expect_enabled"] = True
            elif cmd[1] == "disable":
                self.context["expect_enabled"] = False
            else:
                raise RuntimeError("unknown keyword in expect command")
            return
        elif cmd[0] == "!showmatch":
            if cmd[1] == "enable":
                self.context["showmatch_enabled"] = True
            elif cmd[1] == "disable":
                self.context["showmatch_enabled"] = False
            else:
                raise RuntimeError("unknown keyword in showmatch command")
            return
        elif cmd[0] == "!showlineno":
            if cmd[1] == "enable":
                self.context["showlineno_enabled"] = True
            elif cmd[1] == "disable":
                self.context["showlineno_enabled"] = False
            else:
                raise RuntimeError("unknown keyword in showlineno command")
            return
            
        # TODO: exception type
        raise RuntimeError("unknown metacommand " + repr(cmd))

    def execute_cmd(self, cmd):
        logging.debug("execute_cmd "+repr(cmd))
        if cmd.startswith("!"):
            self.parse_metacommand(cmd)
            return

        client = self.get_active_client()
        client.execute_cmd(cmd, expect_enabled=self.context["expect_enabled"])
        self.last_command_client = self.get_active_client()
        return

    re_script_token = re.compile(
        r"""
# put it in one big group
(
    # universal newline
    \r\n?|\n
    # whitespace, possibly followed by newline
    |[ \t\f\v]+(?:\r\n?|\n)?
    # Python expression invocation
    |[$][{](?:.|\n)*?[}][$]
    # with optional result
     (?:[~][{](?:.|\n)*?[}][~])?
    # Variable substitution
    |[$][a-zA-Z_][a-zA-Z0-9_]*
    # Command or metacommand invocation
    |[>]{3}
    # Comment
    |[#][{](?:.|\n)*?[}][#]
)
""",
        re.VERBOSE,
        )

    re_expr_invocation = re.compile(
        r"""
[$][{]((?:.|\n)*?)[}][$]
[~][{]((?:.|\n)*?)[}][~]?
""",
        re.VERBOSE,
        )

    def parse_script_file(self, filename):
        with open(filename, "r") as f:
            # read entire script into memory
            script_str = f.read()
        self.parse_script(script_str)
        return

    def parse_script(self, script_str):
        in_command = False
        cmd_text = []
        
        parse_pos = 0
        
        for tok in token_iterator(self.re_script_token, script_str):

            t = tok.text
            logging.debug("process token: "+repr(t[:40]))
            if t == "":
                # empty token
                pass
            elif t == ">>>":
                # start of command
                # process before echo so finish_cmd() echoes remaining
                #    output first
                self.finish_cmd()
                in_command = True
                # echo
                if self.context["showlineno_enabled"]:
                    self.print_output("L"+str(tok.lineno)+": ")
                self.print_output(t)
            elif t[-1] in "\r\n":
                # newline
                if in_command:
                    in_command = False
                    self.execute_cmd("".join(cmd_text).strip())
                    del cmd_text[:]
                self.print_output(t)
            elif t[0] in " \t\f\v":
                # whitespace
                if in_command:
                    cmd_text.append(t)
                self.print_output(t)
            elif len(t) >= 4 and t[0:2] == "${":
                # Python expression
                if t[-2:] == "}$":
                    expr = t[2:-2]
                else:
                    mo = self.re_expr_invocation.match(t)
                    if mo is None:
                        # TODO:  better error handling here
                        raise RuntimeError("mismatched ${")
                    expr = mo.group(1)
                self.interpret_expr(expr)
                self.print_output("${"+expr+"}$")
                self.dump_matchbuf(self.context["showmatch_enabled"])
            elif len(t) >= 2 and t[0] == "$" and t[1] in "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_":
                # variable substitution
                name = t[1:]
                value = self.context[name]
                s_value = str(value)
                self.context["matchbuf"].append(s_value)
                if in_command:
                    cmd_text.append(s_value)
                else:
                    self.expect_str(s_value)
                self.print_output(t)
                self.dump_matchbuf(self.context["showmatch_enabled"])
            elif len(t) >= 4 and t[0:2] == "#{" and t[-2:] == "}#":
                # comment
                print(t, end="", flush=True)
            else:
                if in_command:
                    cmd_text.append(t)
                    print(t, end="", flush=True)
                else:
                    self.expect_literal_str(t)
                self.dump_matchbuf(False)
        return

    def run_testdir(self, testdir):
        filenames = sorted(os.listdir(testdir))
        for f in filenames:
            if not f.endswith(".btstest"):
                continue
            self.parse_script_file(os.path.join(testdir, f))
        return

    def register_client(self, client=None):
        self.name2client[client.name] = client
        return

    def finish_cmd(self):
        if self.last_command_client is None:
            return
        end_text = self.last_command_client.finish_cmd()
        if end_text.strip() != "":
            if self.context["expect_enabled"] and self.context["showmatch_enabled"]:
                # TODO:  respect showmatch here
                self.print_output("!{ }!~{ " + end_text.strip() + " }~\n")
            else:
                self.print_output(end_text)
        self.last_command_client = None
        return

    def print_output(self, text):
        print(text, end="", flush=True)
        return

def main():
    parser = argparse.ArgumentParser(description="Testing the future of banking.")
    parser.add_argument("testdirs", metavar="TEST", type=str, nargs="+", help="Test directory")
    parser.add_argument("--testenv", metavar="ENV", type=str, action="append", help="Specify a global testenv")
    parser.add_argument("--loglevel", metavar="LOGLEVEL", type=str, default="INFO")
    parser.add_argument("--client", metavar="CLIENT", type=str, default="", help="Path to BitShares client")
    
    args = parser.parse_args()

    numeric_loglevel = getattr(logging, args.loglevel.upper(), None)
    if not isinstance(numeric_loglevel, int):
        raise ValueError("Invalid log level {}".format(repr(numeric_loglevel)))
    logging.basicConfig(level=numeric_loglevel)

    if args.client != "":
        ClientProcess.default_client_exe = args.client

    for d in args.testdirs:
        local_testenv_filename = os.path.join(d, "testenv")
        if not os.path.exists(local_testenv_filename):
            logging.warn("test "+d+" does not appear to be a test, skipping")
            continue

        test = Test()
        # load all testenv's
        for testenv_filename in (args.testenv or []):
            test.load_testenv(testenv_filename)
        # and local testenv
        test.load_testenv(local_testenv_filename)

        # we're actually done, since local testenv actually runs the test!
    return

if __name__ == "__main__":
    main()

