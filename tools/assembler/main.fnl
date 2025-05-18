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
          proc-lines (preproc.run orig-text)
          token-lines (scanning.run proc-lines)
          (labels instrs) (parsing.parse-tokens token-lines)
          (vars instrs-no-set) (parsing.parse-setvars labels instrs)
          (exp-instrs) (parsing.expand-vars vars instrs-no-set)
          (cat-instrs) (parsing.categorize-opds exp-instrs)
          (res-instrs) (codegen.resolve-names cat-instrs)
          (sized-instrs) (codegen.size-instrs res-instrs)
          (addr-instrs) (codegen.allocate-addr sized-instrs)
          (no-lbl-instrs) (codegen.resolve-labels addr-instrs)
          bytelist (codegen.gen-bytes no-lbl-instrs)]
      (print "Binary size:" (length bytelist))
      (var bytestr "")
      (each [_ by (ipairs bytelist)]
        (set bytestr (.. bytestr (string.char by))))
      (assert (= (length bytestr) (length bytelist)))
      (fout:write bytestr)
      ;
      )))

(main (parse-args arg))

