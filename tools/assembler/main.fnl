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
          (error (.. "Unknown option: " argval))
          (if (= 1 argidx)
              (tset opts :input-file argval)
              (error (.. "Unexpected positional argument at " argidx ": "
                         argval)))))
    opts))

(fn main [{: input-file}]
  (with-open [file (io.open input-file)]
    (let [orig-text (file:read :*all)
          proc-lines (preproc.run orig-text)
          token-lines (scanning.run proc-lines)
          (labels instrs) (parsing.parse-tokens token-lines)
          (vars instrs-no-set) (parsing.parse-setvars labels instrs)
          (exp-instrs) (parsing.expand-vars vars instrs-no-set)
          (cat-instrs) (parsing.categorize-opds exp-instrs)
          (res-instrs) (codegen.resolve-names cat-instrs)
          (sized-instrs) (codegen.size-instrs res-instrs)
          ]
      (do)
      (print (fennel.view sized-instrs))
      ;
      )))

(main (parse-args arg))

