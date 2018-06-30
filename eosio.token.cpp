/**
 *  Copyright (c) 2018 Dylan Phoon
 *  Copyright defined in eosio.token.minable/LICENSE.txt
 *
 *  Copyright defined in eos/LICENSE.txt
 */

#include "eosio.token.hpp"

namespace eosio {

void token::mine( string        nonce,
                  asset         target_token, 
                  account_name  miner )
{
	  require_auth( miner );

    eosio_assert( is_account( miner ), "to account does not exist");
    auto sym = target_token.symbol.name();
    stats statstable( _self, sym );
    const auto& st = statstable.get( sym );

    eosio_assert( target_token.symbol == st.supply.symbol, "symbol precision mismatch" );


    //////////////////////////////////////////////////////////////////////////////////////////
    //    Sets `difficulty` to the difficulty that is
    //    in the relative currency table, then splits
    //    up the array into a larger array of 64 uint32
    //    so that it can be compared to the guess digest.
    //    Also stores the array in a single `BigInteger`
    //    for arithmetic so that difficulty can be set

    BigInteger difficulty;
    BigInteger exponent = 1;
    uint32_t difficulty_to_set[8];
    for ( uint8_t i = 0; i < 8; ++i) {
      difficulty_to_set[i] = st.difficulty[i];
    }
    for (int i = 0; i < 8; ++i) {
      difficulty += (BigInteger)difficulty_to_set[i] * exponent;
      exponent *= 1000;
    }

    BigInteger difficulty_array[64];
    for (int i = 64; i >= 0; i--) {
        difficulty_array[i] = difficulty % 100;
        difficulty /= 100;
    }




    //////////////////////////////////////////////////////////////////////////////////////////
    //    Get the previous hash in the relative 
    //    currency table and combine it with the
    //    guessed nonce into a single string

    string string_previous_hash;
    uint8_t previous_hash[64];
    int previous_hash_size = 64;

    for (int i = 0; i < 64; ++i) {
      previous_hash[i] = st.previous_hash.hash[i];
    }
    for (uint8_t i : previous_hash) {
      string_previous_hash += std::to_string(i);
    }

    char digest[nonce.length()+previous_hash_size]; //The digest that will be hashed
    for (int i = 0; i < sizeof(digest); ++i) {
      if (i >= previous_hash_size) {
        digest[i] = nonce[i-previous_hash_size];
        continue;
      }
      digest[i] = string_previous_hash[i];
    }




    //////////////////////////////////////////////////////////////////////////////////////////
    //    Hashes the digest (sha256) and stores it in result

    checksum256 result;
    sha256( (char *)&digest, sizeof(digest), &result);
    



    //////////////////////////////////////////////////////////////////////////////////////////
    //    Compare the resulting hash generated from the 
    //    nonce and the previous hash with the 
    //    difficulty

    int n = 0;
    n = memcmp ( &result, &difficulty, sizeof(result) );
    eosio_assert(n > 0, "Invalid nonce!");




    //////////////////////////////////////////////////////////////////////////////////////////
    //    Set the current time

    auto time_object = time_point_sec();
    uint32_t now = time_object.sec_since_epoch();

    //////////////////////////////////////////////////////////////////////////////////////////
    //    Modify the currency table
    //      - Update the block height
    //      - Update the last hash
    //      - Set the difficulty

    statstable.modify( st, 0, [&]( auto& s ) {
      s.block_height++;
      s.previous_hash = result;
      s.supply += target_token;
      if (s.block_height % 10 == 0) {
        s.time_since_last_adjustment = now;
        this->set_difficulty();
      } 
    });

    //Rewards the miner
    target_token.set_amount(100);
    add_balance(miner, target_token, miner); 
}

void token::set_difficulty()
{

}

void token::create( account_name issuer,
                    asset        maximum_supply )
{	

	print("Hello Man");
    require_auth( _self );

    auto sym = maximum_supply.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert( maximum_supply.is_valid(), "invalid supply");
    eosio_assert( maximum_supply.amount > 0, "max-supply must be positive");

    stats statstable( _self, sym.name() );
    auto existing = statstable.find( sym.name() );
    eosio_assert( existing == statstable.end(), "token with symbol already exists" );

    statstable.emplace( _self, [&]( auto& s ) {
       s.supply.symbol = maximum_supply.symbol;
       s.max_supply    = maximum_supply;
       s.issuer        = issuer;
    });
}


void token::issue( account_name to, asset quantity, string memo )
{
    auto sym = quantity.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );


    auto sym_name = sym.name();
    stats statstable( _self, sym_name );
    auto existing = statstable.find( sym_name );
    eosio_assert( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
    const auto& st = *existing;

    require_auth( st.issuer );
    eosio_assert( quantity.is_valid(), "invalid quantity" );
    eosio_assert( quantity.amount > 0, "must issue positive quantity" );

    eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    eosio_assert( quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");


    statstable.modify( st, 0, [&]( auto& s ) {
       s.supply += quantity;
    });


    add_balance(to, quantity, to);

    if( to != st.issuer ) {
       SEND_INLINE_ACTION( *this, transfer, {st.issuer,N(active)}, {st.issuer, to, quantity, memo} );
    }
}

void token::transfer( account_name from,
                      account_name to,
                      asset        quantity,
                      string       memo )
{
    eosio_assert( from != to, "cannot transfer to self" );
    require_auth( from );
    eosio_assert( is_account( to ), "to account does not exist");
    auto sym = quantity.symbol.name();
    stats statstable( _self, sym );
    const auto& st = statstable.get( sym );

    require_recipient( from );
    require_recipient( to );

    eosio_assert( quantity.is_valid(), "invalid quantity" );
    eosio_assert( quantity.amount > 0, "must transfer positive quantity" );
    eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );


    sub_balance( from, quantity );
    add_balance( to, quantity, from );
}

void token::sub_balance( account_name owner, asset value ) {
   accounts from_acnts( _self, owner );

   const auto& from = from_acnts.get( value.symbol.name(), "no balance object found" );
   eosio_assert( from.balance.amount >= value.amount, "overdrawn balance" );


   if( from.balance.amount == value.amount ) {
      from_acnts.erase( from );
   } else {
      from_acnts.modify( from, owner, [&]( auto& a ) {
          a.balance -= value;
      });
   }
}

void token::add_balance( account_name owner, asset value, account_name ram_payer )
{
   accounts to_acnts( _self, owner );
   auto to = to_acnts.find( value.symbol.name() );
   if( to == to_acnts.end() ) {
      to_acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = value;
      });
   } else {
      to_acnts.modify( to, 0, [&]( auto& a ) {
        a.balance += value;
      });
   }
}

} /// namespace eosio

EOSIO_ABI( eosio::token, (create)(issue)(transfer)(mine) )
