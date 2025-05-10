(local fennel (require :fennel))

(local preproc (require :preproc))
(local scanning (require :scanning))
(local parsing (require :parsing))

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
          instrs (parsing.run token-lines)
          ;
          ]
      (print (fennel.view instrs))
      ;
      )))

(main (parse-args arg))

