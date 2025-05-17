(local fennel (require :fennel))
(local prs (require :parsing))

(local lib {})

(local mov {:mov_vr {:opd-cats [prs.cat.reg prs.cat.v32] :size 6}
            :mov_rr {:opd-cats [prs.cat.reg prs.cat.reg] :size 2}})

(local str {:str_rv0 {:opd-cats [prs.cat.m32 prs.cat.reg] :size 6}
            :str_ri0 {:opd-cats [prs.cat.ri0 prs.cat.reg] :size 2}
            :str_ri8 {:opd-cats [prs.cat.ri8 prs.cat.reg] :size 3}
            :str_ri32 {:opd-cats [prs.cat.ri32 prs.cat.reg] :size 6}
            :str_rir {:opd-cats [prs.cat.rir prs.cat.reg] :size 3}})

(local ldr {:ldr_rv0 {:opd-cats [prs.cat.reg prs.cat.m32] :size 6}
            :ldr_ri0 {:opd-cats [prs.cat.reg prs.cat.ri0] :size 2}
            :ldr_ri8 {:opd-cats [prs.cat.reg prs.cat.ri8] :size 3}
            :ldr_ri32 {:opd-cats [prs.cat.reg prs.cat.ri32] :size 6}
            :ldr_rir {:opd-cats [prs.cat.reg prs.cat.rir] :size 3}})

(local jmpr {:jmpr_v8 {:opd-cats [prs.cat.v8] :size 2}})

(local jmpa {:jmpa_v32 {:opd-cats [prs.cat.v32] :size 5}
             :jmpa_r {:opd-cats [prs.cat.reg] :size 2}})

(local call {:calla_v32 {:opd-cats [prs.cat.v32] :size 5}
             :calla_r {:opd-cats [prs.cat.reg] :size 2}})

(local ret {:ret {:opd-cats [] :size 1}})

(local halt {:halt {:opd-cats [] :size 1}})
(local iret {:iret {:opd-cats [] :size 1}})

(tset lib :descs {;; Data movement
                  : mov
                  : str
                  : ldr
                  ;; Flow control
                  : jmpr
                  : jmpa
                  : call
                  : ret
                  ;; Other
                  : halt
                  : iret
                  ;;
                  })

lib

