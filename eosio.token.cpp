/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
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

    uint128_t difficulty = st.difficulty;

    uint8_t difficulty_array[64];
    for (int i = 64; i >= 0; i--) {
        difficulty_array[i] = difficulty % 100;
        difficulty /= 100;
    }

    //sizeof nonce + buffer
    char buffer[sizeof(nonce)+hash_size];
    //sets the buffer + nonce to be hashed
    for (int i = 0; i < sizeof(buffer); ++i) {
      if (i >= hash_size) {
        buffer[i] = nonce[i-hash_size];
        continue;
      }
      buffer[i] = a[i];
    }

    //Hashes the buffer and stores it in the result
    checksum256 result;
    sha256( (char *)&buffer, sizeof(buffer), &result);
    
    //if n < 0 then the hash is valid
    //memcmp compares the hash of the buffer to the difficulty
    int n = 0;
    n = memcmp ( &buffer, &difficulty, sizeof(result) );
    eosio_assert(n < 0, "Invalid nonce!");
    add_balance(miner, "100 EOS", miner);    
}

void token::create( account_name issuer,
                    asset        maximum_supply )
{
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
       s.ihash = result;
       s.ihash1 = result2;
        if (n > 0) {
          s.greater = '1';
        } else if ( n < 0) {
          s.greater = '2';
        } else {
          s.greater = '0';
        }
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

EOSIO_ABI( eosio::token, (create)(issue)(transfer) )
