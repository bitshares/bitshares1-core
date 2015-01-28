#!/usr/bin/env python3

# this file is an experiment to run regression tests in parallel
# using Python for cross platform to run with the same script on all platforms
# (currently .sh for Linux, .bat for Windows, ??? for Mac)

# currently it runs 64 (!) regression tests at a time
# apparently each test does not use much memory, I/O or CPU
# -- they appear to be purely bound by sleep()

# on Linux, it is recommended to put output directory on tmpfs, e.g.
# mount -t tmpfs tmpfs ../regression_tests_without_network

import os
import queue
import subprocess
import threading

class TestRunner(object):

    def __init__(self):
        self.basedir = "regression_tests"
        self.q_test = queue.Queue()
        self.q_test_result = queue.Queue()
        self.test_timeout = 5*60
        return

    def list_tests(self):
        test_list = []
        print(os.listdir(self.basedir))
        for regression_test in os.listdir(self.basedir):
            if regression_test.startswith("_"):
               print("failed _")
               continue
            if regression_test.startswith("."):
               print("failed .")
               continue
            if not os.path.isdir(os.path.join(self.basedir, regression_test)):
               print("failed isdir")
               continue
            if not os.path.exists(os.path.join(self.basedir, regression_test, "test.config")):
               print("failed exists test.config")
               continue
            if os.path.exists(os.path.join(self.basedir, regression_test, "wip")):
               print("failed exists wip")
               continue
            test_list.append(regression_test)
        print("here is tests:", test_list)
        return test_list

    def run_regression_test(self, name):
        result = subprocess.call(["./wallet_tests", "-t", "regression_tests_without_network/"+name],
            stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            timeout=self.test_timeout,
            )
        return result

    def run_worker_thread(self):
        while True:
            try:
                test_name = self.q_test.get_nowait()
            except queue.Empty:
                break
            try:
                result = self.run_regression_test(test_name)
            except subprocess.TimeoutExpired:
                result = "TIMEOUT"
            self.q_test_result.put((test_name, result))
        return

    def run_parallel_regression_tests(self, test_list, parallel_jobs=64):
        worker_threads = []
        for test_name in test_list:
            print("adding test:", test_name)
            self.q_test.put(test_name)
        for n in range(parallel_jobs):
            t = threading.Thread(target=self.run_worker_thread, name="test-worker-"+str(n))
            worker_threads.append(t)
            t.start()
        # block and show results
        while True:

            # are there any results?
            while True:
                try:
                    test_name, test_result = self.q_test_result.get(block=False)
                except queue.Empty:
                    break
                if test_result == 0:
                    condition = "passed"
                elif test_result == "TIMEOUT":
                    condition = "timed out"
                else:
                    condition = "failed"
                print("test "+test_name+" "+condition+" (rc="+repr(test_result)+")")

            for t in worker_threads:
                if t.is_alive():
                    t.join(0.050)
                    break
            else:
                # no threads were alive
                break
        return

if __name__ == "__main__":
    runner = TestRunner()
    runner.run_parallel_regression_tests(runner.list_tests())

