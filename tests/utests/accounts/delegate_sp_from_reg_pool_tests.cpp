#include <boost/test/unit_test.hpp>
#include <defines.hpp>
#include <detail.hpp>
#include <db_mock.hpp>

#include <scorum/chain/evaluators/delegate_sp_from_reg_pool_evaluator.hpp>

#include <scorum/chain/dba/db_accessor.hpp>
#include <scorum/chain/services/account.hpp>
#include <scorum/chain/services/dynamic_global_property.hpp>
#include <scorum/chain/services/witness_schedule.hpp>
#include <scorum/chain/services/witness.hpp>

#include <scorum/chain/schema/account_objects.hpp>
#include <scorum/chain/schema/registration_objects.hpp>
#include <scorum/chain/schema/chain_property_object.hpp>

#include <hippomocks.h>

namespace {

using namespace scorum;
using namespace scorum::chain;
using namespace scorum::protocol;
using ::detail::to_hex;

struct delegate_sp_fixture
{
    // TODO: resolve dependencies using boost::di
    delegate_sp_fixture()
        : reg_pool_dba(db)
        , reg_committee_dba(db)
        , reg_pool_delegation_dba(db)
        , chain_dba(db)
        , dprop_svc(db)
        , witness_schedule_svc(db)
        , witness_svc(db, witness_schedule_svc, dprop_svc, chain_dba)
        , account_svc(db, dprop_svc, witness_svc)

    {
        db.add_index<account_index>();
        db.add_index<registration_pool_index>();
        db.add_index<registration_committee_member_index>();
        db.add_index<reg_pool_sp_delegation_index>();
    }

    db_mock db;
    dba::db_accessor<registration_pool_object> reg_pool_dba;
    dba::db_accessor<registration_committee_member_object> reg_committee_dba;
    dba::db_accessor<reg_pool_sp_delegation_object> reg_pool_delegation_dba;
    dba::db_accessor<chain_property_object> chain_dba;

    dbs_dynamic_global_property dprop_svc;
    dbs_witness_schedule witness_schedule_svc;
    dbs_witness witness_svc;
    dbs_account account_svc;

    MockRepository mocks;
    data_service_factory_i* factory = mocks.Mock<data_service_factory_i>();
};

BOOST_FIXTURE_TEST_SUITE(delegate_sp_from_reg_pool_evaluator_tests, delegate_sp_fixture)

SCORUM_TEST_CASE(non_existing_reg_committee_account_should_throw)
{
    delegate_sp_from_reg_pool_operation op;
    op.reg_committee_member = "reg";

    delegate_sp_from_reg_pool_evaluator ev(*factory, account_svc, reg_pool_dba, reg_committee_dba,
                                           reg_pool_delegation_dba);

    SCORUM_CHECK_EXCEPTION(ev.do_apply(op), fc::assert_exception, R"(Account "reg" must exist.)");
}

SCORUM_TEST_CASE(non_existing_delegatee_account_should_throw)
{
    db.create<account_object>([](account_object& o) { o.name = "reg"; });

    delegate_sp_from_reg_pool_operation op;
    op.reg_committee_member = "reg";
    op.delegatee = "delegatee";

    delegate_sp_from_reg_pool_evaluator ev(*factory, account_svc, reg_pool_dba, reg_committee_dba,
                                           reg_pool_delegation_dba);

    SCORUM_CHECK_EXCEPTION(ev.do_apply(op), fc::assert_exception, R"(Account "delegatee" must exist.)");
}

SCORUM_TEST_CASE(specified_reg_committee_member_isnt_reg_committee_member_should_throw)
{
    db.create<account_object>([](account_object& o) { o.name = "reg"; });
    db.create<account_object>([](account_object& o) { o.name = "delegatee"; });

    delegate_sp_from_reg_pool_operation op;
    op.reg_committee_member = "reg";
    op.delegatee = "delegatee";

    delegate_sp_from_reg_pool_evaluator ev(*factory, account_svc, reg_pool_dba, reg_committee_dba,
                                           reg_pool_delegation_dba);

    SCORUM_CHECK_EXCEPTION(ev.do_apply(op), fc::assert_exception, R"(Account 'reg' is not committee member.)");
}

SCORUM_TEST_CASE(not_yet_created_delegation_delegated_sp_cannot_be_zero_should_throw)
{
    db.create<account_object>([](account_object& o) { o.name = "reg"; });
    db.create<account_object>([](account_object& o) { o.name = "delegatee"; });
    db.create<registration_committee_member_object>([](registration_committee_member_object& o) { o.account = "reg"; });

    delegate_sp_from_reg_pool_operation op;
    op.reg_committee_member = "reg";
    op.delegatee = "delegatee";
    op.scorumpower = ASSET_SP(0);

    delegate_sp_from_reg_pool_evaluator ev(*factory, account_svc, reg_pool_dba, reg_committee_dba,
                                           reg_pool_delegation_dba);

    SCORUM_CHECK_EXCEPTION(ev.do_apply(op), fc::assert_exception, "Account has no delegated SP from registration pool");
}

SCORUM_TEST_CASE(not_enough_cash_in_reg_pool_should_throw)
{
    db.create<account_object>([](account_object& o) { o.name = "reg"; });
    db.create<account_object>([](account_object& o) { o.name = "delegatee"; });
    db.create<registration_pool_object>([](registration_pool_object& o) { o.balance = ASSET_SCR(100); });
    db.create<registration_committee_member_object>([](registration_committee_member_object& o) { o.account = "reg"; });

    delegate_sp_from_reg_pool_operation op;
    op.reg_committee_member = "reg";
    op.delegatee = "delegatee";
    op.scorumpower = ASSET_SP(101);

    delegate_sp_from_reg_pool_evaluator ev(*factory, account_svc, reg_pool_dba, reg_committee_dba,
                                           reg_pool_delegation_dba);

    SCORUM_CHECK_EXCEPTION(ev.do_apply(op), fc::assert_exception, "Registration pool is exhausted.");
}

SCORUM_TEST_CASE(new_delegation_created_check_balances)
{
    // clang-format off
    db.create<account_object>([](account_object& o) { o.name = "reg"; });
    const auto& delegatee = db.create<account_object>([](account_object& o) { o.name = "delegatee"; });
    const auto& pool = db.create<registration_pool_object>([](registration_pool_object& o) { o.balance = ASSET_SCR(100); });
    db.create<registration_committee_member_object>([](registration_committee_member_object& o) { o.account = "reg"; });
    // clang-format on

    delegate_sp_from_reg_pool_operation op;
    op.reg_committee_member = "reg";
    op.delegatee = "delegatee";
    op.scorumpower = ASSET_SP(99);

    delegate_sp_from_reg_pool_evaluator ev(*factory, account_svc, reg_pool_dba, reg_committee_dba,
                                           reg_pool_delegation_dba);

    BOOST_REQUIRE_NO_THROW(ev.do_apply(op));
    BOOST_CHECK_EQUAL(delegatee.received_scorumpower, op.scorumpower);
    BOOST_CHECK_EQUAL(pool.balance.amount, 1u);
    BOOST_CHECK_EQUAL(pool.delegated, op.scorumpower);
}

SCORUM_TEST_CASE(new_delegation_object_should_be_created)
{
    db.create<account_object>([](account_object& o) { o.name = "reg"; });
    db.create<account_object>([](account_object& o) { o.name = "delegatee"; });
    db.create<registration_pool_object>([](registration_pool_object& o) { o.balance = ASSET_SCR(100); });
    db.create<registration_committee_member_object>([](registration_committee_member_object& o) { o.account = "reg"; });

    delegate_sp_from_reg_pool_operation op;
    op.reg_committee_member = "reg";
    op.delegatee = "delegatee";
    op.scorumpower = ASSET_SP(99);

    delegate_sp_from_reg_pool_evaluator ev(*factory, account_svc, reg_pool_dba, reg_committee_dba,
                                           reg_pool_delegation_dba);

    ev.do_apply(op);

    BOOST_REQUIRE(!reg_pool_delegation_dba.is_empty());
    BOOST_CHECK_EQUAL(reg_pool_delegation_dba.get().delegatee, "delegatee");
    BOOST_CHECK_EQUAL(reg_pool_delegation_dba.get().sp.amount, 99u);
}

SCORUM_TEST_CASE(delegation_exists_no_enough_cash_to_delegate_extra_sp)
{
    // clang-format off
    db.create<account_object>([](account_object& o) { o.name = "reg"; });
    db.create<account_object>([](account_object& o) { o.name = "delegatee"; });
    db.create<reg_pool_sp_delegation_object>([](reg_pool_sp_delegation_object& o) { o.delegatee = "delegatee"; o.sp = ASSET_SP(30); });
    db.create<registration_pool_object>([](registration_pool_object& o) { o.balance = ASSET_SCR(9); });
    db.create<registration_committee_member_object>([](registration_committee_member_object& o) { o.account = "reg"; });
    // clang-format on

    delegate_sp_from_reg_pool_operation op;
    op.reg_committee_member = "reg";
    op.delegatee = "delegatee";
    op.scorumpower = ASSET_SP(40);

    BOOST_REQUIRE(reg_pool_dba.get().balance.amount + reg_pool_delegation_dba.get().sp.amount < op.scorumpower.amount);

    delegate_sp_from_reg_pool_evaluator ev(*factory, account_svc, reg_pool_dba, reg_committee_dba,
                                           reg_pool_delegation_dba);

    SCORUM_CHECK_EXCEPTION(ev.do_apply(op), fc::assert_exception, "Registration pool is exhausted.");
}

SCORUM_TEST_CASE(update_existing_delegation_increase_account_received_sp)
{
    // clang-format off
    db.create<account_object>([](account_object& o) { o.name = "reg"; });
    const auto& delegatee = db.create<account_object>([](account_object& o) { o.name = "delegatee"; o.received_scorumpower = ASSET_SP(30); });
    db.create<reg_pool_sp_delegation_object>([](reg_pool_sp_delegation_object& o) { o.delegatee = "delegatee"; o.sp = ASSET_SP(30); });
    const auto& pool = db.create<registration_pool_object>([](registration_pool_object& o) { o.balance = ASSET_SCR(20); });
    db.create<registration_committee_member_object>([](registration_committee_member_object& o) { o.account = "reg"; });
    // clang-format on

    delegate_sp_from_reg_pool_operation op;
    op.reg_committee_member = "reg";
    op.delegatee = "delegatee";
    op.scorumpower = ASSET_SP(40);

    delegate_sp_from_reg_pool_evaluator ev(*factory, account_svc, reg_pool_dba, reg_committee_dba,
                                           reg_pool_delegation_dba);

    BOOST_REQUIRE_NO_THROW(ev.do_apply(op));
    BOOST_CHECK_EQUAL(delegatee.received_scorumpower.amount, 40u);
    BOOST_CHECK_EQUAL(pool.balance.amount, 10u);
    BOOST_CHECK_EQUAL(reg_pool_delegation_dba.get().sp.amount, 40u);
}

SCORUM_TEST_CASE(update_existing_delegation_decrease_account_received_sp)
{
    // clang-format off
    db.create<account_object>([](account_object& o) { o.name = "reg"; });
    const auto& delegatee = db.create<account_object>([](account_object& o) { o.name = "delegatee"; o.received_scorumpower = ASSET_SP(30); });
    db.create<reg_pool_sp_delegation_object>([](reg_pool_sp_delegation_object& o) { o.delegatee = "delegatee"; o.sp = ASSET_SP(30); });
    const auto& pool = db.create<registration_pool_object>([](registration_pool_object& o) { o.balance = ASSET_SCR(20); o.delegated = ASSET_SP(10);});
    db.create<registration_committee_member_object>([](registration_committee_member_object& o) { o.account = "reg"; });
    // clang-format on

    delegate_sp_from_reg_pool_operation op;
    op.reg_committee_member = "reg";
    op.delegatee = "delegatee";
    op.scorumpower = ASSET_SP(20);

    delegate_sp_from_reg_pool_evaluator ev(*factory, account_svc, reg_pool_dba, reg_committee_dba,
                                           reg_pool_delegation_dba);

    BOOST_REQUIRE_NO_THROW(ev.do_apply(op));
    BOOST_CHECK_EQUAL(delegatee.received_scorumpower.amount, 20u);
    BOOST_CHECK_EQUAL(pool.balance.amount, 30u);
    BOOST_CHECK_EQUAL(pool.delegated.amount, 0u);
    BOOST_CHECK_EQUAL(reg_pool_delegation_dba.get().sp.amount, 20u);
}

SCORUM_TEST_CASE(remove_existing_delegation_with_zero_delegation)
{
    // clang-format off
    db.create<account_object>([](account_object& o) { o.name = "reg"; });
    const auto& delegatee = db.create<account_object>([](account_object& o) { o.name = "delegatee"; o.received_scorumpower = ASSET_SP(30); });
    db.create<reg_pool_sp_delegation_object>([](reg_pool_sp_delegation_object& o) { o.delegatee = "delegatee"; o.sp = ASSET_SP(30); });
    const auto& pool = db.create<registration_pool_object>([](registration_pool_object& o) { o.balance = ASSET_SCR(20); });
    db.create<registration_committee_member_object>([](registration_committee_member_object& o) { o.account = "reg"; });
    // clang-format on

    delegate_sp_from_reg_pool_operation op;
    op.reg_committee_member = "reg";
    op.delegatee = "delegatee";
    op.scorumpower = ASSET_SP(0);

    delegate_sp_from_reg_pool_evaluator ev(*factory, account_svc, reg_pool_dba, reg_committee_dba,
                                           reg_pool_delegation_dba);

    BOOST_REQUIRE_NO_THROW(ev.do_apply(op));
    BOOST_CHECK_EQUAL(delegatee.received_scorumpower.amount, 0u);
    BOOST_CHECK_EQUAL(pool.balance.amount, 50u);
    BOOST_CHECK(reg_pool_delegation_dba.is_empty());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE(delegate_sp_from_reg_pool_operation_tests, delegate_sp_fixture)

SCORUM_TEST_CASE(delegatee_and_reg_committee_member_should_be_different_accounts)
{
    delegate_sp_from_reg_pool_operation op;
    op.delegatee = "sam";
    op.reg_committee_member = "sam";

    SCORUM_CHECK_EXCEPTION(op.validate(), fc::assert_exception, "You cannot delegate SP to yourself");
}

SCORUM_TEST_CASE(delegated_sp_upper_bound_test)
{
    delegate_sp_from_reg_pool_operation op;
    op.delegatee = "sam";
    op.reg_committee_member = "alice";
    op.scorumpower = ASSET_SP(10'000'000'000);

    BOOST_CHECK_NO_THROW(op.validate());

    op.scorumpower.amount += 1;

    SCORUM_CHECK_EXCEPTION(op.validate(), fc::assert_exception, "Delegation cannot be more than 10.000000000 SP");
}

SCORUM_TEST_CASE(serialize_delegate_sp_from_reg_pool_to_hex)
{
    delegate_sp_from_reg_pool_operation op;
    op.reg_committee_member = "initdelegate";
    op.delegatee = "alice";
    op.scorumpower = ASSET_SP(10'000'000'000);

    scorum::protocol::operation ops = op;
    auto hex = to_hex(ops);

    BOOST_CHECK_EQUAL(hex, "2a0c696e697464656c656761746505616c69636500e40b54020000000953500000000000");
}

BOOST_AUTO_TEST_SUITE_END()
}
