#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <cmath>
#include <string>

namespace cnyg_token
{

    using std::string;
    using namespace eosio;

    static constexpr int16_t RATIO_BOOST        = 1'0000;
    static constexpr int16_t CNYG_PRECISION     = 1'0000;
    static constexpr eosio::symbol CNYG_SYMBOL  = symbol("CNYG", 4);
    static constexpr eosio::name active_perm    {"active"_n};
  
    /**
     * The `cnyg.token` sample system contract defines the structures and actions that allow users to create, issue, and manage tokens for AMAX based blockchains. It demonstrates one way to implement a smart contract which allows for creation and management of tokens. It is possible for one to create a similar contract which suits different needs. However, it is recommended that if one only needs a token with the below listed actions, that one uses the `cnyg.token` contract instead of developing their own.
     *
     * The `cnyg.token` contract class also implements two useful public static methods: `get_supply` and `get_balance`. The first allows one to check the total supply of a specified token, created by an account and the second allows one to check the balance of a token for a specified account (the token creator account has to be specified as well).
     *
     * The `cnyg.token` contract manages the set of tokens, accounts and their corresponding balances, by using two internal multi-index structures: the `accounts` and `stats`. The `accounts` multi-index table holds, for each row, instances of `account` object and the `account` object holds information about the balance of one token. The `accounts` table is scoped to an eosio account, and it keeps the rows indexed based on the token's symbol.  This means that when one queries the `accounts` multi-index table for an account name the result is all the tokens that account holds at the moment.
     *
     * Similarly, the `stats` multi-index table, holds instances of `currency_stats` objects for each row, which contains information about current supply, maximum supply, and the creator account for a symbol token. The `stats` table is scoped to the token symbol.  Therefore, when one queries the `stats` table for a token symbol the result is one single entry/row corresponding to the queried symbol token if it was previously created, or nothing, otherwise.
     * The `cnyg.token` is base on `amax.token`, support fee of transfer
     */
    class [[eosio::contract( "cnyg.token" )]] xtoken : public contract
    {
    public:
        using contract::contract;

        xtoken(eosio::name receiver, eosio::name code, datastream<const char*> ds): 
            contract(receiver, code, ds), _global(_self, _self.value) 
        {
            if (_global.exists()) {
                _g = _global.get();

            } else { // first init
                _g = global_t{};
                _g.admin = _self;
            }
        }

        ~xtoken() { _global.set( _g, get_self() ); }

        /**
         *  This action issues to `to` account a `quantity` of tokens.
         *
         * @param to - the account to issue tokens to, it must be the same as the issuer,
         * @param quntity - the amount of tokens to be issued,
         * @memo - the memo string that accompanies the token issue transaction.
         */
        [[eosio::action]] void issue(const name &to, const asset &quantity, const string &memo);

        /**
         * The opposite for create action, if all validations succeed,
         * it debits the statstable.supply amount.
         *
         * @param quantity - the quantity of tokens to retire,
         * @param memo - the memo string to accompany the transaction.
         */
        [[eosio::action]] void retire(const asset &quantity, const string &memo);

        /**
         * Allows `from` account to transfer to `to` account the `quantity` tokens.
         * One account is debited and the other is credited with quantity tokens.
         *
         * @param from - the account to transfer from,
         * @param to - the account to be transferred to,
         * @param quantity - the quantity of tokens to be transferred,
         * @param memo - the memo string to accompany the transaction.
         */
        [[eosio::action]] void transfer(const name &from,
                                        const name &to,
                                        const asset &quantity,
                                        const string &memo);

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
        [[eosio::action]] void notifypayfee(const name &from, const name &to, const name& fee_receiver, const asset &fee, const string &memo);


        /**
         * Pause token
         * If token is paused, users can not do actions: transfer(), open(), close(),
         * @param symbol - the symbol of the token.
         * @param paused - is paused.
         */
        [[eosio::action]] void pause(bool paused);

        /**
         * freeze account
         * If account of token is frozen, it can not do actions: transfer(), close(),
         * @param symbol - the symbol of the token.
         * @param account - account name.
         * @param is_frozen - is account frozen.
         */
        [[eosio::action]] void freezeacct(const name &account, bool is_frozen);

        ACTION feeexempt(const name &account, bool is_fee_exempt);

        static asset get_balance(const name &token_contract_account, const name &owner, const symbol_code &sym_code)
        {
            accounts accountstable(token_contract_account, owner.value);
            const auto &ac = accountstable.get(sym_code.raw());
            return ac.balance;
        }

        using transfer_action = eosio::action_wrapper<"transfer"_n, &xtoken::transfer>;
        using notifypayfee_action = eosio::action_wrapper<"notifypayfee"_n, &xtoken::notifypayfee>;

        ACTION init(const name& issuer, const name& fee_collector, const symbol& symbol) {
            require_auth( _self );
            check( is_account(issuer), "issuer account does not exist" );
            check( is_account(fee_collector), "fee_collector account does not exist" );
            check( !_g.initialized, "already initialized" );

            _g.admin                   = _self;
            _g.issuer                  = issuer;
            _g.fee_collector           = fee_collector;
            _g.max_supply              = asset(100'000'000'000'0000, symbol);
            _g.supply                  = asset(0, symbol);
            _g.initialized             = true;
        }

        ACTION setfeeratio(const uint8_t& fee_ratio) {
            require_auth( _self );

            _g.fee_ratio = fee_ratio;
        }

    private:
      struct [[eosio::table("global"), eosio::contract( "cnyg.token" )]] global_t {
            name admin;                             //_self;
            name fee_collector ;
            uint8_t fee_ratio               = 5;    //boost by 10000
            uint8_t fee_max                 = 50;   //max fee amount: 50 CNYG
            uint8_t fee_start_amount        = 100;  //fee charge starting transfer amount, default: 100 CNYG
            
            asset supply;
            asset max_supply;
            name issuer;
            bool paused = false;
            bool initialized = false;

            EOSLIB_SERIALIZE( global_t, (admin)(fee_collector)(fee_ratio)(fee_max)
                            (fee_start_amount)(supply)(max_supply)(issuer)(paused) )
        };

        typedef eosio::singleton< "global"_n, global_t > global_singleton;
        global_singleton    _global;
        global_t            _g;

    private:
        struct [[eosio::table]] fee_exempt_account 
        {
            name account;

            uint64_t primary_key() const { return account.value; }

            EOSLIB_SERIALIZE( fee_exempt_account, (account) )
        };
        typedef eosio::multi_index<"feeexempts"_n, fee_exempt_account> feeexempt_tbl;

        struct [[eosio::table]] account
        {
            asset balance;
            bool  is_frozen = false;

            uint64_t primary_key() const { return balance.symbol.code().raw(); }

            EOSLIB_SERIALIZE( account, (balance)(is_frozen) )
        };
        typedef eosio::multi_index<"accounts"_n, account> accounts;

    private:
        void sub_balance(const name &owner, const asset &value, bool frozen_check_required = false);
        void add_balance(const name &owner, const asset &value, const name &ram_payer, bool frozen_check_required = false);

        inline bool is_account_frozen(const name &owner, const account &acct) const {
            return acct.is_frozen && owner != _g.issuer;
        }

        bool is_account_fee_exempted( const name& account ) {
            feeexempt_tbl accts(_self, _self.value);
            return( accts.find( account.value ) != accts.end() );
        }

    };

}
