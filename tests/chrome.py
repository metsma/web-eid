# Copyright (C) 2017 Martin Paljak

import json
import subprocess
import struct
import sys
import os
import unittest
import testconf
import uuid

class ChromeTest(unittest.TestCase):

  def instruct(self, msg):
      global input
      try:
        input = raw_input
      except NameError:
        pass
      input('>>>>>> %s\n[press ENTER to continue]' % msg)

  def get_response(self):
      response_length = struct.unpack("=I", self.p.stdout.read(4))[0]
      response = str(self.p.stdout.read(response_length))
      # make it into "oneline" json before printing
      json_object = json.loads(response)
      print("RECV: %s" % json.dumps(json_object))
      return json_object

  def transceive(self, msg):
      return self.transceive_dumb(json.dumps(msg))

  def transact(self, msg):
      if not "id" in msg: msg["id"] = str(uuid.uuid4())
      if not "origin" in msg: msg["origin"] = "https://example.com"
      response = self.transceive(msg)
      self.assertEqual(response["id"], msg["id"])
      self.assertEqual("error" in response, False)
      return response

  def transceive_dumb(self, msg):
      print("SEND: %s" % msg)
      self.p.stdin.write(struct.pack("=I", len(msg)))
      self.p.stdin.write(bytearray(msg, 'utf-8'))
      return self.get_response()

  def setUp(self):
      should_close_fds = sys.platform.startswith('win32') == False;
      self.p = subprocess.Popen([testconf.get_exe(), "chrome-extension://fmpfihjoladdfajbnkdfocnbcehjpogi", "--debug"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, close_fds=should_close_fds, stderr=None)
      print("\nRunning %s on PID %d" % (testconf.get_exe(), self.p.pid))
      if "HWDEBUG" in os.environ:
        self.instruct("Start testing")

  def tearDown(self):
      if self.p.poll() == None:
          self.p.terminate()
