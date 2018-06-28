/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <iostream>
#include <eosiolib/asset.hpp>
#include <eosiolib/print.hpp>
#include <utility>
#include <vector>
#include <string>
#include <eosiolib/eosio.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/contract.hpp>
#include <eosiolib/crypto.h>
#include <eosiolib/print.hpp>  
 #include "../biginteger-for-eosio/BigIntegerLibrary.hpp" 

namespace eosiosystem {
   class system_contract;
}

namespace eosio {

   using std::string;

   class token : public contract {
      public:
         token( account_name self ):contract(self){}
         //@abi action
         void create( account_name issuer,
                      asset        maximum_supply );
         //@abi action
         void issue( account_name to, asset quantity, string memo );
         //@abi action
         void transfer( account_name from,
                        account_name to,
                        asset        quantity,
                        string       memo );

         void mine ( 	string 			nonce,
         				asset         	target_token,  
         				account_name 	miner );
      
      
         inline asset get_supply( symbol_name sym )const;
         
         inline asset get_balance( account_name owner, symbol_name sym )const;

      private:
         struct account {
            asset    balance;

            uint64_t primary_key()const { return balance.symbol.name(); }
         };

         struct currency_stats {
            asset          	supply;
            asset          	max_supply;
            account_name   	issuer;
            uint128_t		difficulty = 0xffffff;
            uint64_t primary_key()const { return supply.symbol.name(); }
         };

         typedef eosio::multi_index<N(accounts), account> accounts;
         typedef eosio::multi_index<N(stat), currency_stats> stats;

         void sub_balance( account_name owner, asset value );
         void add_balance( account_name owner, asset value, account_name ram_payer );

      public:
         struct transfer_args {
            account_name  from;
            account_name  to;
            asset         quantity;
            string        memo;
         };
   };

   asset token::get_supply( symbol_name sym )const
   {
      stats statstable( _self, sym );
      const auto& st = statstable.get( sym );
      return st.supply;
   }

   asset token::get_balance( account_name owner, symbol_name sym )const
   {
      accounts accountstable( _self, owner );
      const auto& ac = accountstable.get( sym );
      return ac.balance;
   }

} /// namespace eosio
