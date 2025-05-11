(local tok (require :scanning))
(local fennel (require :fennel))

(local lib {})

(local is-in-list? (fn [list val]
                     "Returns 'true' if 'list' contains 'val', otherwise returns 'false'."
                     (var found false)
                     (each [_ v (ipairs list) &until found]
                       (set found (= v val)))
                     found))

(fn lib.parse-tokens [token-lines]
  "Parses each of 'token-lines' into a list of instructions."
  (local all-labels {})

  (fn filter-comments [tokens]
    (icollect [_ token (ipairs tokens)]
      (when (not= tok.type.comment (. token :type)) token)))

  (fn parse-line [tokens]
    "Parses 'tokens' into an instruction.

    'tokens' must have the following format ([] means optional, / means one of):
    [id colon] id      [id/string/number [comma id/string/number [comma ...]]]
    ^label     ^opcode  ^operand 1              ^operand 2

    Appends a label to 'all-labels' if 'tokens' contains a label.
    "
    (let [has-label? (and (>= (length tokens) 2)
                          (= tok.type.id (. tokens 1 :type))
                          (= tok.type.colon (. tokens 2 :type)))
          opcode-idx (if has-label? 3 1)
          has-opcode? (>= (length tokens) opcode-idx)
          label (when has-label? (. tokens 1 :val))
          opcode-type (?. tokens opcode-idx :type)
          opcode (when has-opcode?
                   (if (= tok.type.id opcode-type)
                       (. tokens opcode-idx :val)
                       (error (.. "expected token type id, got " opcode-type ""
                                  " on token-line:\n" (fennel.view tokens)))))
          first-opd-idx (+ 1 opcode-idx)
          operands {}]
      (when has-label? (table.insert all-labels label))
      (for [i first-opd-idx (length tokens)]
        ;; 'An operand' means here any token after the opcode, including a
        ;; comma.
        (let [opd-num (- i first-opd-idx)
              opd-tok (. tokens i)
              opd-type (. opd-tok :type)
              need-comma? (= 1 (% opd-num 2)) ; every second token = comma
              ]
          (if need-comma?
              (when (not= tok.type.comma opd-type)
                (error (.. "expected a comma token at index " i ":\n"
                           (fennel.view tokens))))
              (if (or (= tok.type.id opd-type) (= tok.type.string opd-type)
                      (= tok.type.number opd-type))
                  (table.insert operands opd-tok)
                  (error (.. "expected token type id, string or number, got "
                             opd-type " on token-line:\n" (fennel.view tokens)))))))
      (values (or has-label? has-opcode?) {: label : opcode : operands})))

  (values all-labels (icollect [_ tokens (ipairs token-lines)]
                       (let [(not-empty? instr) (parse-line (filter-comments tokens))]
                         (when not-empty? instr)))))

(fn lib.parse-setvars [labels instrs]
  "Returns variables defined by '.set' pseudo-instructios in 'instrs'."
  (local var-names {})
  (local vars {})

  (fn parse-setvar [instr]
    "Appends a variable name and value defined by 'instr' to 'vars'.

    A variable name must be a string (token type string).
    A variable value may be one of:
    - another variable name (token type id and an existing value in 'vars'), in
      which case the value variable is expanded,
    - label (token type id and an existing value in 'labels'),
    - string (token type string),
    - number (token type number).
    "
    (assert (= instr.opcode :.set))
    (if (not= (length instr.operands) 2)
        (error (.. "wrong number of operands for .set: expected 2, got "
                   (length instr.operands) " in instruction:\n"
                   (fennel.view instr))))
    (let [tok-name (. instr :operands 1)
          tok-val (. instr :operands 2)]
      (if (not= tok-name.type tok.type.id)
          (error (.. "wrong type of variable name: expected id, got "
                     tok-name.type " in instruction:\n" (fennel.view instr))))
      (if (and (not= tok-val.type tok.type.id)
               (not= tok-val.type tok.type.string)
               (not= tok-val.type tok.type.number))
          (error (.. "wrong type of variable value: expected label/string/"
                     "number, got " tok-name.type " in instruction:\n"
                     (fennel.view instr))))
      (when (and (= tok-val.type tok.type.id)
                 (not (or (is-in-list? var-names tok-val.val)
                          (is-in-list? labels tok-val.val))))
        (error (.. "variable '" tok-name.val
                   "' is set to a non-existent variable or label " "'"
                   tok-val.val "' in instruction:\n" (fennel.view instr))))
      (if (is-in-list? var-names tok-val.val)
          (do
            ;; Add a variable with the name 'tok-name.val' and set it to the
            ;; value of an existing variable.
            (table.insert var-names tok-name.val)
            (tset vars tok-name.val (. vars tok-val.val)))
          (do
            ;; Add a new variable.
            (table.insert var-names tok-name.val)
            (tset vars tok-name.val tok-val)))))

  (values vars (icollect [_ instr (ipairs instrs)]
                 (if (and instr.opcode (= instr.opcode :.set))
                     (parse-setvar instr)
                     instr))))

(fn lib.expand-vars [vars instrs]
  "Expands variable identifiers in 'instrs' if they are present in 'vars'.

  - 'vars' is a table of variables, where the key is a variable name and the
    value is the value token.
  - 'instrs' is a list of instruction tables, where each table is a list of
    tokens.
  "
  (local var-names (icollect [var-name _ (pairs vars)] var-name))

  (fn expand-instr [instr]
    (for [i 1 (length instr.operands)]
      (let [opd (. instr :operands i)]
        (when (and (= opd.type tok.type.id) (is-in-list? var-names opd.val))
          (tset opd :type (. vars opd.val :type))
          (tset opd :val (. vars opd.val :val)))))
    instr)

  (icollect [_ instr (ipairs instrs)]
    (expand-instr instr)))

lib

