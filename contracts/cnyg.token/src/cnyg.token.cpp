#include <cnyg.token/cnyg.token.hpp>
using namespace std;
namespace cnyg_token {

#ifndef ASSERT
    #define ASSERT(exp) eosio::check(exp, #exp)
#endif

#define CHECK(exp, msg) { if (!(exp)) eosio::check(false, msg); }

    template<typename Int, typename LargerInt>
    LargerInt multiply_decimal(LargerInt a, LargerInt b, LargerInt precision) {
        LargerInt tmp = a * b / precision;
        CHECK(tmp >= std::numeric_limits<Int>::min() && tmp <= std::numeric_limits<Int>::max(),
            "overflow exception of multiply_decimal");
        return tmp;
    }

    #define mul64(a, b, precision) multiply_decimal<int64_t, int128_t>(a, b, precision)

    void xtoken::issue(const name &issuer, const asset &quantity, const string &memo)
    {
        require_auth(issuer);

        check(issuer == _g.issuer, "non-issuer auth failure");
        check(memo.size() <= 256, "memo has more than 256 bytes");
        check(quantity.is_valid(), "invalid quantity");
        check(quantity.amount > 0, "must issue positive quantity");
        check(quantity.symbol == _g.max_supply.symbol, "symbol precision mismatch");
        check(quantity.amount <= _g.max_supply.amount - _g.supply.amount, "quantity exceeds available supply");

        _g.supply          += quantity;

        add_balance(_g.issuer, quantity, _g.issuer);
    }

    void xtoken::retire(const asset &quantity, const string &memo)
    {
        const auto& sym = quantity.symbol;
        auto sym_code_raw = sym.code().raw();
        check(sym.is_valid(), "invalid symbol name");
        check(memo.size() <= 256, "memo has more than 256 bytes");

        require_auth(_g.issuer);

        check(quantity.is_valid(), "invalid quantity");
        check(quantity.amount > 0, "must retire positive quantity");

        check(quantity.symbol == _g.supply.symbol, "symbol precision mismatch");

        _g.supply               -= quantity;

        sub_balance(_g.issuer, quantity);
    }

    void xtoken::transfer(const name &from,
                          const name &to,
                          const asset &quantity,
                          const string &memo)
    {
        require_auth(from);

        check(from != to, "cannot transfer to self");
        check(is_account(to), "to account does not exist");
        check(_g.supply.symbol == quantity.symbol, "symbol precision mismatch");
        check(!_g.is_paused, "token is paused");
        check(quantity.is_valid(), "invalid quantity");
        check(quantity.amount > 0, "must transfer positive quantity");
        check(memo.size() <= 256, "memo has more than 256 bytes");

        require_recipient(from);
        require_recipient(to);

        asset actual_recv   = quantity;
        asset fee           = asset(0, quantity.symbol);
        if (    _g.fee_collector.value != 0 &&  _g.fee_ratio > 0
            &&  to != _g.issuer &&  to != _g.fee_collector 
            &&  quantity.amount >= _g.fee_start_amount * pow(10,_g.supply.symbol.precision()))
        {
            accounts to_accts(get_self(), to.value);
            auto sym_code_raw = quantity.symbol.code().raw();
            auto to_acct = to_accts.find(sym_code_raw);
            if ( to_acct == to_accts.end() || !to_acct->is_fee_exempt)
            {
                fee.amount =  std::min<int64_t>( _g.fee_max * pow(10,_g.supply.symbol.precision()) , (int64_t)mul64(quantity.amount, _g.fee_ratio, RATIO_BOOST) );
                CHECK(fee < quantity, "the calculated fee must less than quantity");
                actual_recv -= fee;
            }
        }

        auto payer = has_auth(to) ? to : from;

        sub_balance(from, quantity, true);
        add_balance(to, actual_recv, payer, true);

        if (fee.amount > 0) {
            add_balance( _g.fee_collector, fee, payer );
            notifypayfee_action notifypayfee_act{ get_self(), { {get_self(), active_perm} } };
            notifypayfee_act.send( from, to, _g.fee_collector, fee, memo );
        }
    }

    /**
     * Notify pay fee.
     * Must be Triggered as inline action by transfer()
     *
     * @param from - the from account of transfer(),
     * @param to - the to account of transfer, fee payer,
     * @param fee_receiver - fee receiver,
     * @param fee - the fee of transfer to be payed,
     * @param memo - the memo of the transfer().
     * Require contract auth
     */
    void xtoken::notifypayfee(const name &from, const name &to, const name& fee_receiver, const asset &fee, const string &memo) {
        require_auth(get_self());
        require_recipient(to);
        require_recipient(fee_receiver);
    }

    void xtoken::sub_balance(const name &owner, const asset &value, bool frozen_check_required)
    {
        accounts from_accts(get_self(), owner.value);
        const auto &from = from_accts.get(value.symbol.code().raw(), "no balance object found");
        check(!frozen_check_required || !is_account_frozen(owner, from), "from account is frozen");
        check(from.balance.amount >= value.amount, "overdrawn balance");

        from_accts.modify(from, owner, [&](auto &a) {
            a.balance -= value;
        });
    }

    void xtoken::add_balance(const name &owner, const asset &value, const name &ram_payer, bool frozen_check_required)
    {
        accounts to_accts(get_self(), owner.value);
        auto to = to_accts.find(value.symbol.code().raw());
        if (to == to_accts.end()) {
            to_accts.emplace(ram_payer, [&](auto &a) {
                a.balance = value;
            });
            return;
        }

        check( !frozen_check_required || !is_account_frozen(owner, *to), "to account is frozen" );

        to_accts.modify(to, same_payer, [&](auto &a) {
            a.balance += value;
        });
    }

    void xtoken::feeexempt(const name &account, bool is_fee_exempt) {
        require_auth(_g.issuer);
        
        auto sym_code_raw = _g.supply.symbol.code().raw();

        accounts accts(get_self(), account.value);
        const auto &acct = accts.get(sym_code_raw, "account of token does not exist");

        accts.modify(acct, _g.issuer, [&](auto &a) {
             a.is_fee_exempt = is_fee_exempt;
        });
    }

    void xtoken::pause(bool is_paused)
    {
        _g.is_paused = is_paused;
    }

    void xtoken::freezeacct(const name &account, bool is_frozen) {
        require_auth(_g.issuer);

        accounts accts(get_self(), account.value);
        auto sym_code_raw = _g.supply.symbol.code().raw();
        const auto &acct = accts.get(sym_code_raw, "account of token does not exist");

        accts.modify(acct, _g.issuer, [&](auto &a) {
             a.is_frozen = is_frozen;
        });
    }

} /// namespace amax_xtoken
