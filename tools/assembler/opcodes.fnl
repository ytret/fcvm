(local fennel (require :fennel))

(local lib {})

(tset lib :descs [])

(macro add-variant* [desc-tbl name [instr-fmt desc]]
  (assert-compile (sequence? instr-fmt) "expected sequence for instr-fmt"
                  instr-fmt)
  (assert-compile (table? desc) "expected table for desc" desc)
  (let [[opcode opd-list] instr-fmt
        {:match fn-match : size} desc]
    (assert-compile (= :string (type opcode)) "expected string for opcode"
                    opcode)
    (assert-compile (list? opd-list) "expected list for opd-list" opd-list)
    (assert-compile (list? fn-match) "expected list for match" desc)
    (assert-compile (= :number (type size)) "epxected number for size" desc)
    `(tset (. ,desc-tbl ,name) ,opcode
           {:match (fn []
                     (do
                       ))
            :size ,size})))

(macro add-desc* [desc-tbl name ...]
  (assert-compile (sym? desc-tbl) "expected symbol for desc-tbl" desc-tbl)
  (assert-compile (= :string (type name)) "expected string for name" name)
  (assert-compile (not= 0 (select "#" ...)) "expected at least one variant")
  (let [var-forms []]
    (each [i variant (ipairs (pack ...))]
      (assert-compile (list? variant) (.. "expected list for variant #" i)
                      variant)
      (table.insert var-forms
                    (macroexpand `(add-variant* ,desc-tbl ,name ,variant))))
    `(do
       (tset ,desc-tbl ,name {})
       ,(unpack var-forms))))

(add-desc* lib.descs :mov
           ([:mov_vr (o1 o2)] {:match (and o1.is-dreg? o2.is-imm32?) :size 6})
           ([:mov_rr (o1 o2)] {:match (and o1.is-dreg? o2.is-dreg?) :size 2}))

(print (fennel.view lib))

lib

