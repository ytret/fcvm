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
      ;; operands with category [lbl]; here we treat them as [v8 v32].
      (let [instr-opd-cats (match-exact* instr-opd.cats ;;
                                         [prs.cat.lbl] [prs.cat.v8 prs.cat.v32]
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

  - Non-pseudo-instructions always have non-zero size and increment the address
    by their size.
  - Some pseudo-instructions (e.g., '.origin', '.skip') that are present at this
    stage change the address and get removed from 'instrs'.
  - Other pseudo-instructions must have size.
  "
  (var addr 0)

  (fn alloc-addr-instr [instr]
    (fn do-addr-manip [instr]
      "Performs an address manipulation instruction 'instr' on 'addr.

      'instr' must be either an '.origin' or '.skip' instruction.
      "
      (assert (= :string (type instr.name)))
      (match instr.name
        :.origin (do
                   (assert (not= nil instr.operands))
                   (assert (= 1 (length instr.operands)))
                   (assert (is-in-list? (. instr.operands 1 :cats) prs.cat.v32))
                   (let [new-addr (. instr.operands 1 :val)]
                     (when (< new-addr addr)
                       (error (.. ".origin tried to lower the address from "
                                  addr " to " new-addr " :\n"
                                  (fennel.view instr))))
                     (set addr new-addr)))
        :.skip (do
                 (assert (not= nil instr.operands))
                 (assert (= 1 (length instr.operands)))
                 (assert (is-in-list? (. instr.operands 1 :cats) prs.cat.v32))
                 (set addr (+ addr (. instr.operands 1 :val))))
        _ (error (.. "invalid instruction for do-addr-manip:\n"
                     (fennel.view instr)))))

    (let [has-name? (not= nil instr.name)
          has-size? (not= nil instr.size)
          is-addr-manip? (is-in-list? [:.origin :.skip] instr.name)]
      (when (and has-name? (not is-addr-manip?) (not has-size?))
        (error (.. "instruction '" instr.name "' must have size:\n"
                   (fennel.view instr))))
      (if is-addr-manip?
          (do-addr-manip instr)
          (do
            (tset instr :addr addr)
            (when has-size?
              (set addr (+ addr instr.size)))
            instr))))

  (icollect [_ instr (ipairs instrs)]
    (alloc-addr-instr instr)))

(fn lib.resolve-labels [instrs]
  "Resolves category 'prs.cat.lbl' operands in 'instrs'.

  Sets the 'res-val' field in the operand table to the resolved address.
  Removes 'label' field from the instructions; if the instruction does not have
  a name, removes it from the list.

  Special case: labels used as an operand to relative control flow instructions
  are resolved relative to the instruction address.
  "
  (fn build-label-map [instrs]
    (let [label-map {}]
      (each [_ instr (ipairs instrs)]
        (when (not= nil instr.label)
          (tset label-map instr.label instr.addr)))
      label-map))

  (fn resolve-lbl-opds [label-map instr]
    (each [_ opd (ipairs instr.operands)]
      (when (is-in-list? opd.cats prs.cat.lbl)
        (let [lbl-addr (?. label-map opd.val)
              instr-addr instr.addr
              val-abs lbl-addr
              val-rel (- lbl-addr instr-addr)
              use-rel? (is-in-list? [:jmpr :jeqr :jner] instr.name)]
          (if (= nil lbl-addr)
              (error (.. "cannot resolve label '" opd.val "' in:\n"
                         (fennel.view instr)))
              (tset opd :res-val (if use-rel? val-rel val-abs)))))))

  (let [label-map (build-label-map instrs)]
    (icollect [_ instr (ipairs instrs)]
      (do
        (when (not= nil instr.operands)
          (resolve-lbl-opds label-map instr))
        (tset instr :label nil)
        (when (not= nil instr.name) instr)))))

(fn lib.gen-bytes [instrs]
  (local bytes [])

  (fn add-instr [instr]
    (fn add-byte [by]
      (assert (or (and (<= -127 by) (<= by 127)) (and (<= 0 by) (<= by 255))))
      (table.insert bytes (if (< by 0) (+ by 256) by)))

    (fn add-bytes [...]
      (each [_ by (ipairs (table.pack ...))]
        (add-byte by)))

    (fn add-dword [val]
      (let [signed-min (- (lshift 1 31))
            signed-max (- (lshift 1 31) 1)
            unsigned-min 0
            unsigned-max (- (lshift 1 32) 1)
            abs-val (if (>= val 0) val (- val))]
        (assert (or (and (<= signed-min val) (<= val signed-max))
                    (and (<= unsigned-min val) (<= val unsigned-max))))
        (add-bytes (band (rshift abs-val 0) 255) ;
                   (band (rshift abs-val 8) 255) ;
                   (band (rshift abs-val 16) 255) ;
                   (band (rshift abs-val 24) 255))))

    (fn add-lbl-or-val [fn-add opd]
      (let [is-label? (is-in-list? opd.cats prs.cat.lbl)]
        (fn-add (if is-label? opd.res-val opd.val))))

    (fn add-str [val]
      (each [ch (string.gmatch val ".")]
        (add-byte (string.byte ch)))
      (add-byte 0))

    (fn reg-code [reg-name]
      (let [reg-codes {:r0 0 :r1 1 :r2 2 :r3 3 :r4 4 :r5 5 :r6 6 :r7 7 :sp 8}
            reg-code (?. reg-codes reg-name)]
        (when (= nil reg-code)
          (error (.. "unrecognized register '" (fennel.view reg-name) "'")))
        reg-code))

    (fn reg-codes [reg1-name reg2-name]
      (let [reg1-code (reg-code reg1-name)
            reg2-code (reg-code reg2-name)]
        (values reg1-code reg2-code)))

    (fn add-opds [instr desc-opd-cats]
      (let [o1 (?. instr :operands 1)
            o2 (?. instr :operands 2)
            ;; match won't let us use multi symbols like 'prs.cat.reg', so we
            ;; use aliases
            c-reg prs.cat.reg
            c-v5 prs.cat.v5
            c-v8 prs.cat.v8
            c-v32 prs.cat.v32
            c-m32 prs.cat.m32
            c-ri0 prs.cat.ri0
            c-ri8 prs.cat.ri8
            c-ri32 prs.cat.ri32
            c-rir prs.cat.rir]
        (match-exact* desc-opd-cats ;;
                      [c-reg c-m32]
                      (do
                        (add-byte (reg-code o1.val))
                        (add-dword o2.val.val.val))
                      [c-m32 c-reg]
                      (do
                        (add-byte (reg-code o2.val))
                        (add-dword o1.val.val.val))
                      [c-reg c-reg] (add-bytes (reg-codes o2.val o1.val))
                      [c-reg c-ri0]
                      (add-bytes (reg-codes o1.val o2.val.val.val))
                      [c-ri0 c-reg]
                      (add-bytes (reg-codes o2.val o1.val.val.val))
                      [c-reg c-ri8]
                      (do
                        (add-byte (reg-code o1.val))
                        (add-byte (reg-code o2.val.lhs.val))
                        (add-byte o2.val.rhs.val))
                      [c-ri8 c-reg]
                      (do
                        (add-byte (reg-code o2.val))
                        (add-byte (reg-code o1.val.lhs.val))
                        (add-byte o1.val.rhs.val))
                      [c-reg c-ri32]
                      (do
                        (add-byte (reg-code o1.val))
                        (add-byte (reg-code o2.val.lhs.val))
                        (add-dword o2.val.rhs.val))
                      [c-ri32 c-reg]
                      (do
                        (add-byte (reg-code o2.val))
                        (add-byte (reg-code o1.val.lhs.val))
                        (add-dword o1.val.rhs.val))
                      [c-rir c-reg]
                      (do
                        (add-byte (reg-code o2.val))
                        (add-byte (reg-code o1.val.lhs.val))
                        (add-byte (reg-code o1.val.rhs.val)))
                      [c-reg c-rir]
                      (do
                        (add-byte (reg-code o1.val))
                        (add-byte (reg-code o2.val.lhs.val))
                        (add-byte (reg-code o2.val.rhs.val)))
                      [c-reg c-v32]
                      (do
                        (add-byte (reg-code o1.val))
                        (add-lbl-or-val add-dword o2))
                      [c-reg c-v5]
                      (do
                        (add-byte (reg-code o1.val))
                        (add-byte o2.val)) ;
                      [c-v32] (add-lbl-or-val add-dword o1) ;
                      [c-v8] (add-lbl-or-val add-byte o1) ;
                      [c-reg] (add-byte (reg-code o1.val)) ;
                      _ (error (.. "cannot encode operands in:\n"
                                  (fennel.view instr))))))

    (let [pad-size (- instr.addr (length bytes))]
      (assert (>= pad-size 0)
              (.. "bad size calculation, cannot put instruction at its address "
                  "below " (length bytes) ":\n" (fennel.view instr)))
      (for [i 1 pad-size]
        (table.insert bytes 0)))
    (local size-before (length bytes))
    (if (= nil instr.gen-desc)
        (case instr.name
          :.strs (do
                   (assert (not= nil instr.operands))
                   (assert (= 1 (length instr.operands)))
                   (assert (is-in-list? (. instr.operands 1 :cats) prs.cat.str))
                   (add-str (. instr.operands 1 :val)))
          :.strd (do
                   (assert (not= nil instr.operands))
                   (assert (= 1 (length instr.operands)))
                   (add-lbl-or-val add-dword (. instr.operands 1)))
          :.strb (do
                   (assert (not= nil instr.operands))
                   (assert (= 1 (length instr.operands)))
                   (assert (is-in-list? (. instr.operands 1 :cats) prs.cat.v8))
                   (add-byte (. instr.operands 1 :val)))
          _ (error (.. "unrecognized instruction without gen-desc:\n"
                       (fennel.view instr))))
        (do
          (add-byte instr.gen-desc.opcode)
          (when (> (length instr.gen-desc.opd-cats) 0)
            (add-opds instr instr.gen-desc.opd-cats))))
    (local size-after (length bytes))
    (assert (= (- size-after size-before) instr.size)
            (.. "bad encoded size, expected " instr.size ", got "
                (- size-after size-before) ":\n" (fennel.view instr))))

  (assert (not= 0 (length instrs)))
  (each [_ instr (ipairs instrs)]
    (add-instr instr))
  bytes)

(fn lib.run [instrs]
  "Generates bytecode (a sequential table of byte-sized integers) for 'instrs'."
  (-> instrs
      (lib.resolve-names)
      (lib.size-instrs)
      (lib.allocate-addr)
      (lib.resolve-labels)
      (lib.gen-bytes)))

lib

