(import-macros {: enum* : match-exact*} :auxm)
(local tok (require :scanning))
(local fennel (require :fennel))

(local lib {;; Type of an instruction or memory access sub-operand.
            ;; - mov r1, 2
            ;;       ^id ^number
            ;; - ldr r1, [r2 + 2]
            ;;       ^id  ^id  ^number
            :opd (enum* [:id :string :number :mem])
            ;; Type of a memory access operand.
            ;; - ldr r1, [0xCC00FFEE]  :no-off (i.e. without an offset)
            ;; - ldr r1, [r2 + 3]      :off (i.e. with an offset)
            :mem-type (enum* [:no-off :off])})

(local is-in-list? (fn [list val]
                     "Returns 'true' if 'list' contains 'val', otherwise returns 'false'."
                     (var found false)
                     (each [_ v (ipairs list) &until found]
                       (set found (= v val)))
                     found))

(local parse-number
       (fn [num-str]
         (case num-str
           (where x (string.match (string.lower x) "^0x[%x]+"))
           (tonumber (string.sub x 3) 16)
           (where x (string.match x "^[%d]+")) (tonumber x 10))))

(local parse-indir-id-id (fn [tok-base tok-op tok-off]
                           {}))

(fn lib.parse-tokens [token-lines]
  "Parses each of 'token-lines' into a list of instructions."
  (local all-labels {})

  (fn filter-comments [tokens]
    (icollect [_ token (ipairs tokens)]
      (when (not= tok.type.comment (. token :type)) token)))

  (fn parse-line [tokens]
    "Parses 'tokens' into an instruction table.

    Appends a label name to 'all-labels' if 'tokens' contains a label token.

    'tokens' must have the following format ([] means optional, / means one of):
    - [id colon] id      [opd1       [comma opd2       [comma ...]]]
      ^label     ^name    ^operand 1        ^operand 2

    'opd1', 'opd2', and other operands must be one of:
    - id/string/number  ; A raw label, string, or number.
    - open-sq number close-sq  ; A direct memory address.
    - open-sq id [arithm-op id/number] close-sq  ; An indirect memory address.
    "
    (fn parse-opd [opd-toks]
      (let [tok-types (icollect [_ opd-tok (ipairs opd-toks)] opd-tok.type)
            o1 (?. opd-toks 1)
            o2 (?. opd-toks 2)
            o3 (?. opd-toks 3)
            o4 (?. opd-toks 4)]
        (var is-bad? false)
        (match-exact* tok-types [tok.type.id] {:type lib.opd.id :val o1.val}
                      [tok.type.string] {:type lib.opd.string :val o1.val}
                      [tok.type.number]
                      {:type lib.opd.number :val (parse-number o1.val)}
                      [tok.type.open-sq tok.type.number tok.type.close-sq]
                      {:type lib.opd.mem
                       :val {:type lib.mem-type.no-off
                             :val {:type lib.opd.number
                                   :val (parse-number o2.val)}}}
                      [tok.type.open-sq tok.type.id tok.type.close-sq]
                      {:type lib.opd.mem
                       :val {:type lib.mem-type.no-off
                             :val {:type lib.opd.id :val o2.val}}}
                      [tok.type.open-sq
                       tok.type.id
                       tok.type.arithm-op
                       tok.type.number
                       tok.type.close-sq]
                      {:type lib.opd.mem
                       :val {:type lib.mem-type.off
                             :lhs {:type lib.opd.id :val o2.val}
                             :op o3.val
                             :rhs {:type lib.opd.number
                                   :val (parse-number o4.val)}}}
                      [tok.type.open-sq
                       tok.type.id
                       tok.type.arithm-op
                       tok.type.id
                       tok.type.close-sq]
                      {:type lib.opd.mem
                       :val {:type lib.mem-type.off
                             :lhs {:type lib.opd.id :val o2.val}
                             :op o3
                             :rhs {:type lib.opd.id :val o4.val}}}
                      _
                      (error (.. "bad operand tokens:\n" (fennel.view opd-toks))))))

    (let [has-label? (and (= tok.type.id (?. tokens 1 :type))
                          (= tok.type.colon (?. tokens 2 :type)))
          label (when has-label? (. tokens 1 :val))
          name-idx (if has-label? 3 1)
          has-name? (>= (length tokens) name-idx)
          name-type (?. tokens name-idx :type)
          name (when has-name?
                   (if (= tok.type.id name-type)
                       (. tokens name-idx :val)
                       (error (.. "expected token type id, got " name-type ""
                                  " on token-line:\n" (fennel.view tokens)))))
          operands {}]
      (when has-label? (table.insert all-labels label))
      (var curr-opd-toks {})
      (for [i (+ 1 name-idx) (length tokens)]
        (let [curr-tok (. tokens i)]
          (if (= tok.type.comma curr-tok.type)
              (do
                (table.insert operands curr-opd-toks)
                (set curr-opd-toks {}))
              (table.insert curr-opd-toks curr-tok))))
      (when (> (length tokens) name-idx)
        (table.insert operands curr-opd-toks))
      (each [opd-idx opd-toks (ipairs operands)]
        (tset operands opd-idx (parse-opd opd-toks)))
      (values (or has-label? has-name?)
              {: label
               : name
               :operands (when (not= 0 (length operands)) operands)})))

  (values all-labels (icollect [_ tokens (ipairs token-lines)]
                       (let [(not-empty? instr) (parse-line (filter-comments tokens))]
                         (when not-empty? instr)))))

(fn lib.parse-setvars [labels instrs]
  "Parses variables defined by '.set' pseudo-instructions in 'instrs'.

  Each '.set' instruction must have two operands:
  1. Variable name - 'lib.opd.id'.
  2. Variable value.

  Allowed variable value types:
  1. An ID operand ('lib.opd.id') - a label, register or another variable. If
     the token value is a previously encountered variable name, it gets expanded
     to the appropriate variable value.
  2. A string ('lib.opd.string').
  3. A number ('lib.opd.number').

  All other operand types (e.g., 'lib.opd.mem') are forbidden and produce an
  error.

  Returns two values:
  1. Map of variable values: key - variable name (string), value - second
     operand of the '.set' instruction (see above for the allowed types).
  2. Filtered instruction list, i.e. 'instrs' without any '.set' instruction.
  "
  (local var-names {})
  (local vars {})

  (fn parse-setvar [instr]
    "Appends a variable name and value defined by 'instr' to 'vars'.

    Note: 'instr' must be a '.set' instruction.

    For the allowed types of the operands, see above.
    "
    (assert (= instr.name :.set))
    (if (not= (length instr.operands) 2)
        (error (.. "wrong number of operands for .set: expected 2, got "
                   (length instr.operands) " in instruction:\n"
                   (fennel.view instr))))
    (let [opd-name (. instr :operands 1)
          opd-val (. instr :operands 2)]
      (if (not= opd-name.type lib.opd.id)
          (error (.. "wrong type of variable name: expected id, got "
                     opd-name.type " in instruction:\n" (fennel.view instr))))
      (if (and (not= opd-val.type lib.opd.id)
               (not= opd-val.type lib.opd.string)
               (not= opd-val.type lib.opd.number))
          (error (.. "wrong type of variable value: expected variable/label/"
                     "string/number, got " opd-val.type " in instruction:\n"
                     (fennel.view instr))))
      (when (and (= opd-val.type lib.opd.id)
                 (not (or (is-in-list? var-names opd-val.val)
                          (is-in-list? labels opd-val.val))))
        (error (.. "variable '" opd-name.val
                   "' is set to a non-existent variable or label " "'"
                   opd-val.val "' in instruction:\n" (fennel.view instr))))
      (if (is-in-list? var-names opd-val.val)
          (do
            ;; Add a variable with the name 'opd-name.val' and set it to the
            ;; value of an existing variable.
            (table.insert var-names opd-name.val)
            (tset vars opd-name.val (. vars opd-val.val)))
          (do
            ;; Add a new variable.
            (table.insert var-names opd-name.val)
            (tset vars opd-name.val opd-val)))))

  (values vars (icollect [_ instr (ipairs instrs)]
                 (if (and instr.name (= instr.name :.set))
                     (parse-setvar instr)
                     instr))))

(fn lib.expand-vars [vars instrs]
  "Expands variable identifiers in 'instrs' if they are present in 'vars'.

  - 'vars' is a table of variables, where the key is a variable name and the
    value is the value token.
  - 'instrs' is a list of instruction tables.
  "
  (fn expand-opd [vars opd]
    (fn expand-opd [vars opd]
      (let [var-names (icollect [name _ (pairs vars)] name)
            var-val (?. vars opd.val)]
        (when (and (= lib.opd.id opd.type) var-val)
          (tset opd :type var-val.type)
          (tset opd :val var-val.val))))

    (match opd.type
      lib.opd.id (expand-opd vars opd)
      lib.opd.mem (match opd.val.type
                    lib.mem-type.no-off (expand-opd vars opd.val.val)
                    lib.mem-type.off (do
                                       (expand-opd vars opd.val.lhs)
                                       (expand-opd vars opd.val.rhs))
                    _ (error (.. "unknown mem-indir operand type " opd.val.type)))))

  (fn expand-instr [instr]
    (when instr.operands
      (for [i 1 (length instr.operands)]
        (let [opd (. instr :operands i)]
          (expand-opd vars opd))))
    instr)

  (icollect [_ instr (ipairs instrs)]
    (expand-instr instr)))

lib

