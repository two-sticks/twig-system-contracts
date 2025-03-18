void token::issue(const name & owner, const fungible_pairs & tokens, const std::string & memo)
{
  require_auth(get_self());

  _supplies supplies_(get_self(), get_self().value);
  auto supplies = supplies_.get();

  fungible_pairs new_balance;

  _accounts accounts(get_self(), get_self().value);
  auto accounts_itr = accounts.find(owner.value);
  if (accounts_itr != accounts.end()){
    new_balance = accounts_itr->balance;
  }

  for (const std::pair<eosio::symbol, int64_t> & fungible : tokens){
    check(supplies.supply.find(fungible.first) != supplies.supply.end(), "this fungible does not exist: " + fungible.first.code().to_string());
    check(supplies.supply.find(fungible.first)->second + fungible.second < supplies.max_supply.find(fungible.first)->second, "cannot issue above fungible max supply: " + fungible.first.code().to_string());
    check(fungible.second > 0, "cannot issue an amount below 0: " + fungible.first.code().to_string());

    supplies.supply.find(fungible.first)->second += fungible.second;

    if (new_balance.find(fungible.first) == new_balance.end()){
      new_balance.emplace(fungible.first, fungible.second);
    } else {
      new_balance.find(fungible.first)->second += fungible.second;
    }
  }

  if (accounts_itr == accounts.end()){
    accounts.emplace(get_self(), [&](auto & row){
      row.owner = owner;
      row.balance = new_balance;
    });
  } else {
    accounts.modify(accounts_itr, get_self(), [&](auto & row){
      row.balance = new_balance;
    });
  }

  supplies_.set(supplies, get_self());
}

void token::retire(const name & owner, const fungible_pairs & tokens, const std::string & memo)
{
  require_auth(get_self());

  _supplies supplies_(get_self(), get_self().value);
  auto supplies = supplies_.get();

  _accounts accounts(get_self(), get_self().value);
  auto accounts_itr = accounts.require_find(owner.value, "owner account not found");
  fungible_pairs new_balance = accounts_itr->balance;

  for (const std::pair<eosio::symbol, int64_t> & fungible : tokens){
    check(supplies.supply.find(fungible.first) != supplies.supply.end(), "this fungible does not exist: " + fungible.first.code().to_string());
    check(supplies.supply.find(fungible.first)->second - fungible.second >= 0, "cannot retire below zero: " + fungible.first.code().to_string());
    check(fungible.second > 0, "cannot retire an amount below 0: " + fungible.first.code().to_string());

    supplies.supply.find(fungible.first)->second -= fungible.second;

    auto balance_itr = new_balance.find(fungible.first);
    check(balance_itr != new_balance.end(), "owner does not have any of this fungible: " + fungible.first.code().to_string());
    check(balance_itr->second - fungible.second >= 0, "owner does not have enough of this fungible: " + fungible.first.code().to_string());

    new_balance.find(fungible.first)->second -= fungible.second;
    if (new_balance.find(fungible.first)->second == 0){
      new_balance.erase(fungible.first);
    }
  }

  accounts.modify(accounts_itr, get_self(), [&](auto & row){
    row.balance = new_balance;
  });

  supplies_.set(supplies, get_self());
}

void token::rolldrops(const name & owner, const drop_params & params)
{
  require_auth(get_self());

  /* quantum mechanics :D */
}

void token::execdrops(const checksum256 & seed)
{
  require_auth(get_self());

  /* quantum mechanics :D */
}
