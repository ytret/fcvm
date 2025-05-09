#include <gtest/gtest.h>

#include <fcvm/intctl.h>

#define TEST_INVALID_IRQ (INTCTL_MAX_IRQ_NUM + 1)

class IntCtlTest : public testing::Test {
  protected:
    IntCtlTest() {
        intctl = intctl_new();
    }
    ~IntCtlTest() {
        intctl_free(intctl);
    }

    intctl_ctx_t *intctl;
};

TEST_F(IntCtlTest, InitHasNoPendingIRQs) {
    EXPECT_EQ(intctl_has_pending_irqs(intctl), false);

    uint8_t irq = 0xFF;
    EXPECT_EQ(intctl_get_pending_irq(intctl, &irq), false);
    EXPECT_EQ(irq, 0xFF);
}

TEST_F(IntCtlTest, RaiseInvalidIRQ) {
    vm_err_t err = intctl_raise_irq_line(intctl, TEST_INVALID_IRQ);
    EXPECT_EQ(err, VM_ERR_INVALID_IRQ_NUM);

    uint8_t pending_irq;
    EXPECT_EQ(intctl_has_pending_irqs(intctl), false);
    EXPECT_EQ(intctl_get_pending_irq(intctl, &pending_irq), false);
}

TEST_F(IntCtlTest, RaiseSingleIRQ) {
    uint8_t raised_irq = 0;
    vm_err_t err = intctl_raise_irq_line(intctl, raised_irq);
    EXPECT_EQ(err, VM_ERR_NONE);

    uint8_t pending_irq = ~raised_irq;
    EXPECT_EQ(intctl_has_pending_irqs(intctl), true);
    EXPECT_EQ(intctl_get_pending_irq(intctl, &pending_irq), true);
    EXPECT_EQ(pending_irq, raised_irq);

    EXPECT_EQ(intctl_has_pending_irqs(intctl), false);
    EXPECT_EQ(intctl_get_pending_irq(intctl, &pending_irq), false);
}

TEST_F(IntCtlTest, LowerNumIsHigherPriority) {
    std::array<uint8_t, INTCTL_MAX_IRQ_NUM + 1> raise_order = {
        6,  7,  9,  3, 16, 0,  22, 13, 21, 27, 15, 18, 8,  1,  14, 4,
        26, 30, 17, 2, 12, 19, 31, 29, 5,  28, 25, 11, 10, 24, 23, 20,
    };
    for (uint8_t irq : raise_order) {
        vm_err_t err = intctl_raise_irq_line(intctl, (uint8_t)irq);
        EXPECT_EQ(err, VM_ERR_NONE);
    }

    for (uint8_t irq = 0; irq < INTCTL_MAX_IRQ_NUM; irq++) {
        uint8_t pending_irq = 0xFF;
        EXPECT_EQ(intctl_has_pending_irqs(intctl), true);
        EXPECT_EQ(intctl_get_pending_irq(intctl, &pending_irq), true);
        EXPECT_EQ(pending_irq, irq);
    }
}

TEST_F(IntCtlTest, SnapshotRestore) {
    static_assert(SN_INTCTL_CTX_VER == 1);

    uint8_t raised_irq = 1;
    vm_err_t err = intctl_raise_irq_line(intctl, raised_irq);
    EXPECT_EQ(err, VM_ERR_NONE);

    size_t snapshot_size = intctl_snapshot_size();
    uint8_t *snapshot_buf = new uint8_t[snapshot_size];
    size_t used_size = intctl_snapshot(intctl, snapshot_buf, snapshot_size);
    EXPECT_EQ(used_size, snapshot_size);

    size_t rest_size = 0;
    intctl_ctx_t *rest_intctl =
        intctl_restore(snapshot_buf, used_size, &rest_size);
    EXPECT_EQ(rest_size, used_size);

    EXPECT_EQ(rest_intctl->raised_irqs, intctl->raised_irqs);

    delete[] snapshot_buf;
}
