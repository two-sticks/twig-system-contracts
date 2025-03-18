void token::cfginit(const std::string & memo)
{
  require_auth(get_self());

  _config config_(get_self(), get_self().value);
  config_.set(_config_s{}, get_self());

  _fungibles fungibles_(get_self(), get_self().value);
  fungibles_.set(_fungibles_s{}, get_self());

  _supplies supplies_(get_self(), get_self().value);
  supplies_.set(_supplies_s{}, get_self());
}

void token::cfgdestruct(const std::string & memo)
{
  require_auth(get_self());

  _config config_(get_self(), get_self().value);
  config_.remove();

  _fungibles fungibles_(get_self(), get_self().value);
  fungibles_.remove();

  _supplies supplies_(get_self(), get_self().value);
  supplies_.remove();
}

void token::cfgsetparams(const config_params & params, const std::string & memo)
{
  require_auth(get_self());

  _config config_(get_self(), get_self().value);
  auto config = config_.get();

  config.params = params;

  config_.set(config, get_self());
}

void token::setfungibles(const fungible_params & params, const std::string & memo)
{
  require_auth(get_self());

  _fungibles fungibles_(get_self(), get_self().value);
  auto fungibles = fungibles_.get();

  fungibles.params = params;

  fungibles_.set(fungibles, get_self());
}

void token::setsupplies(const fungible_pairs & max_supply, const std::string & memo)
{
  require_auth(get_self());

  _supplies supplies_(get_self(), get_self().value);
  auto supplies = supplies_.get();

  for (const std::pair<eosio::symbol, int64_t> & fungible : max_supply){
    check(supplies.max_supply.find(fungible.first) == supplies.max_supply.end(), "this fungible already exists: " + fungible.first.code().to_string());
    asset check_asset(fungible.second, fungible.first);

    check(check_asset.is_valid(), "invalid supply");
    check(check_asset.amount > 0, "max-supply must be positive");
    supplies.max_supply.emplace(fungible.first, fungible.second);
    supplies.supply.emplace(fungible.first, (int64_t)0);
  }

  supplies_.set(supplies, get_self());
}

void token::rmvsupply(const fungible_pairs & max_supply, const std::string & memo)
{
  require_auth(get_self());

  _supplies supplies_(get_self(), get_self().value);
  auto supplies = supplies_.get();

  for (const std::pair<eosio::symbol, int64_t> & fungible : max_supply){
    check(supplies.max_supply.find(fungible.first) != supplies.max_supply.end(), "this fungible doesn't exist: " + fungible.first.code().to_string());
    check(supplies.supply.find(fungible.first)->second == 0, "this fungible has a non-zero supply: " + fungible.first.code().to_string());

    supplies.max_supply.erase(fungible.first);
    supplies.supply.erase(fungible.first);
  }
  supplies_.set(supplies, get_self());
}

void token::setdrops(const uint64_t index, const fungible_drops & params, const std::string & memo)
{
  require_auth(get_self());

  _drops drops(get_self(), get_self().value);
  auto drops_itr = drops.find(index);

  if (drops_itr == drops.end()){
    drops.emplace(get_self(), [&](auto & row){
      row.index = index;
      row.params = params;
    });
  } else {
    drops.modify(drops_itr, get_self(), [&](auto & row){
      row.index = index;
      row.params = params;
    });
  }
}
void token::rmvdrops(const uint64_t index)
{
  require_auth(get_self());

  _drops drops(get_self(), get_self().value);
  auto drops_itr = drops.find(index);

  if (drops_itr != drops.end()){
    drops.erase(drops_itr);
  }
}