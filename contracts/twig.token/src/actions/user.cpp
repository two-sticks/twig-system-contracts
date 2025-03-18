void token::open(const name & owner)
{
  check(has_auth(get_self()) || has_auth(owner), "missing auth");

  _accounts accounts(get_self(), get_self().value);
  auto accounts_itr = accounts.find(owner.value);
  if (accounts_itr == accounts.end()){
    accounts.emplace(get_self(), [&](auto & row){
      row.owner = owner;
    });
  }
}

void token::close(const name & owner)
{
  check(has_auth(get_self()) || has_auth(owner), "missing auth"); // temp
  // require_auth(owner);

  _accounts accounts(get_self(), get_self().value);
  auto accounts_itr = accounts.require_find(owner.value, "owner account not found");

  for (const std::pair<eosio::symbol, int64_t> & fungible : accounts_itr->balance){
    check(fungible.second == 0, "cannot close an account with a non-zero fungible balance: " + fungible.first.code().to_string());
  }

  accounts.erase(accounts_itr);
}
void token::claimdrops(const u64v & indices, const name & owner)
{
  check(has_auth(get_self()) || has_auth(owner), "missing auth");

  fungible_pairs new_tokens;
  _unboxed unboxed(get_self(), get_self().value);

  for (const uint64_t & index : indices){
    auto unboxed_itr = unboxed.require_find(index, "index not found");
    check(unboxed_itr->owner == owner, "missing ownership");
    const auto & tokens = unboxed_itr->tokens;
    for (const std::pair<eosio::symbol, int64_t> & fungible : tokens){
      if (new_tokens.find(fungible.first) == new_tokens.end()){
        new_tokens.emplace(fungible.first, fungible.second);
      } else {
        new_tokens.find(fungible.first)->second += fungible.second;
      }
    }
    unboxed.erase(unboxed_itr);
  }
  eosio::action(eosio::permission_level{get_self(), name("active")}, get_self(), name("issue"),
    std::make_tuple(
      owner,
      new_tokens,
      "claiming token drops"
    )).send();
}

void token::transfer(const name & from, const name & to, const std::vector<asset> & tokens, const string & memo)
{
  require_auth(from);
  check(from != to, "cannot transfer to self");
  check(is_account(to), "to account does not exist");
  check(memo.size() <= 256, "memo has more than 256 bytes");
  check(tokens.size() > 0 && tokens.size() < 100, "invalid tokens.size()");

  require_recipient(from);
  require_recipient(to);

  _accounts accounts(get_self(), get_self().value);
  auto from_itr = accounts.require_find(from.value, "from account not found");
  auto to_itr = accounts.find(to.value);

  fungible_pairs from_balance = from_itr->balance;
  fungible_pairs to_balance;
  if (to_itr != accounts.end()){
    to_balance = to_itr->balance;
  }

  for (const asset & fungible : tokens){
    check(fungible.is_valid(), "invalid quantity: " + fungible.symbol.code().to_string());
    check(fungible.amount > 0, "must transfer positive quantity: " + fungible.symbol.code().to_string());

    check(from_balance.find(fungible.symbol) != from_balance.end(), "owner does not have this fungible: " + fungible.symbol.code().to_string());
    check(from_balance.find(fungible.symbol)->second - fungible.amount >= 0, "owner does not have enough of this fungible: " + fungible.symbol.code().to_string());

    from_balance.find(fungible.symbol)->second -= fungible.amount;
    if (from_balance.find(fungible.symbol)->second == 0){
      from_balance.erase(fungible.symbol);
    }

    if (to_balance.find(fungible.symbol) == to_balance.end()){
      to_balance.emplace(fungible.symbol, fungible.amount);
    } else {
      to_balance.find(fungible.symbol)->second += fungible.amount;
    }
  }

  accounts.modify(from_itr, get_self(), [&](auto & row){
    row.balance = from_balance;
  });

  if (to_itr == accounts.end()){
    accounts.emplace(get_self(), [&](auto & row){
      row.owner = to;
      row.balance = to_balance;
    });
  } else {
    accounts.modify(to_itr, get_self(), [&](auto & row){
      row.balance = to_balance;
    });
  }
}