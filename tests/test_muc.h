void muc_before_test(void **state);
void muc_after_test(void **state);

void test_muc_add_invite(void **state);
void test_muc_remove_invite(void **state);
void test_muc_invite_count_0(void **state);
void test_muc_invite_count_5(void **state);
void test_muc_room_is_not_active(void **state);
void test_muc_room_is_active(void **state);
