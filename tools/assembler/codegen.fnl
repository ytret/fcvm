(import-macros {: match-exact*} :auxm)
(local {: is-in-list?} (require :auxfn))
(local fennel (require :fennel))
(local opcodes (require :opcodes))
(local prs (require :parsing))

(local lib {})

(fn lib.resolve-names [instrs]
  "Resolves each non-pseudo-instruction in 'instrs' to a single opcode.

  Uses opcode descriptors from 'opcodes.descs'. Sets the key 'gen-desc' of
  'instr' to the matched opcode descriptor.
  "
  (fn resolve-name [instr]
    (local num-instr-opds
           (if (= nil instr.operands) 0 (length instr.operands)))

    (fn opd-matches? [desc-opd-cat instr-opd]
      "Returns true if the operand 'instr-opd' matches with the operand category
      'desc-opd-cat'."
      ;; HACK: at this stage labels are still not resolved and are stored as
      ;; operands with category [id]; here we treat them as [id v32].
      (let [instr-opd-cats (match-exact* instr-opd.cats ;;
                                         [prs.cat.id] [prs.cat.id prs.cat.v32]
                                         ;;
                                         _ instr-opd.cats)]
        (is-in-list? instr-opd-cats desc-opd-cat)))

    (fn opcode-matches? [desc instr]
      (let [num-desc-opds (length desc.opd-cats)
            same-num-opds (= num-desc-opds num-instr-opds)
            opds-match (faccumulate [opds-match true i 1 num-desc-opds
                                     &until (not opds-match)]
                         (and opds-match
                              (opd-matches? (. desc.opd-cats i)
                                            (. instr.operands i))))]
        (and same-num-opds opds-match)))

    (assert (not= nil instr.name))
    (when (= nil (?. opcodes :descs instr.name))
      (error (.. "unrecognized instruction name '" instr.name "' in:\n"
                 (fennel.view instr))))
    (var resolved? false)
    (each [_ desc (pairs (. opcodes.descs instr.name)) &until resolved?]
      (when (opcode-matches? desc instr)
        (tset instr :gen-desc desc)
        (set resolved? true)))
    (when (not resolved?)
      (error (.. "unrecognized instruction:\n" (fennel.view instr))))
    instr)

  (icollect [_ instr (ipairs instrs)]
    (if (or (= nil instr.name) (= 1 (string.find instr.name "%.")))
        instr
        (resolve-name instr))))

(fn lib.size-instrs [instrs]
  "Appends to almost all instructions in 'instrs' a 'size' field.

  - Non-pseudo-instructions have an already determined size in the 'gen-desc'
    field; it is propagated to the 'size' field.
  - Some pseudo-instructions (e.g., '.origin') are processed at a later stage
    and don't have a size.
  - Other pseudo-instructions (e.g., '.strb') have their size set in the 'size'
    field.
  "
  (fn get-instr-size [instr]
    (if (= nil instr.gen-desc)
        (match instr.name
          :.origin nil
          :.skip nil
          :.strd 4
          :.strw 2
          :.strb 1
          :.strs (do
                   (assert (not= nil instr.operands))
                   (assert (= 1 (length instr.operands)))
                   (assert (= prs.cat.str (. instr.operands 1 :cats 1)))
                   (+ 1 (string.len (. instr.operands 1 :val))))
          _ (error (.. "cannot determine size of instruction:\n"
                       (fennel.view instr))))
        (. instr.gen-desc :size)))

  (each [_ instr (ipairs instrs)]
    (when (not= nil instr.name)
      (let [size (get-instr-size instr)]
        (tset instr :size size))))
  instrs)

(fn lib.allocate-addr [instrs]
  "Adds to each instruction in 'instrs' its address in bytes.

  Initial address is zero.

  - Non-pseudo-instructions always have a non-zero size and increment the
    address by their size.
  - Pseudo-instructions (e.g., '.origin', '.strb', '.skip') that are present at
    this stage change the address. Some of them, like '.origin', are removed
    from 'instrs'.
  "
  (var addr 0)

  (fn alloc-addr-instr [instr])

  (icollect [_ instr (ipairs instrs)]
    (alloc-addr-instr instr)))

lib

