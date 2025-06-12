#!/usr/bin/env fennel

(local fennel (require :fennel))

(local preproc (require :preproc))
(local scanning (require :scanning))
(local parsing (require :parsing))
(local codegen (require :codegen))

(fn parse-args [args]
  "Parses a table of string arguments 'args' and returns a table."
  (let [opts {}]
    (each [argidx argval (ipairs args)]
      (if (= "-" (string.sub argval 0 1))
          (error (.. "unknown option: " argval))
          (match argidx
            1 (tset opts :input-file argval)
            2 (tset opts :output-file argval)
            _ (error (.. "unexpected positional argument at " argidx ": "
                         argval)))))
    opts))

(fn main [{: input-file : output-file}]
  (assert (not= nil input-file) "no input file provided")
  (assert (not= nil output-file) "no output file provided")
  (with-open [fin (io.open input-file) fout (io.open output-file :wb)]
    (let [orig-text (fin:read :*all)
          prog-bytes (-> orig-text
                         (preproc.run)
                         (scanning.run)
                         (parsing.run)
                         (codegen.run))]
      (print "Binary size:" (length prog-bytes))
      (var bytestr "")
      (each [_ by (ipairs prog-bytes)]
        (set bytestr (.. bytestr (string.char by))))
      (assert (= (length bytestr) (length prog-bytes)))
      (fout:write bytestr))))

(main (parse-args arg))

