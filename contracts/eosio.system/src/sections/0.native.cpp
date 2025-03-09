void system::check_auth_change(name contract, name account, const binary_extension<name> & authorized_by)
{
  name by(authorized_by.has_value() ? authorized_by.value().value : 0);
  if(by.value){
    require_auth({account, by});
  }

  _limitauthchg table(contract, contract.value);
  auto itr = table.find(account.value);
  if(itr == table.end()){
    return;
  }
  check(by.value, "authorized_by is required for this account");
  if(!itr->allow_perms.empty()){
    check(
      std::find(itr->allow_perms.begin(), itr->allow_perms.end(), by) != itr->allow_perms.end(),
      "authorized_by does not appear in allow_perms");
  } else {
    check(
      std::find(itr->disallow_perms.begin(), itr->disallow_perms.end(), by) == itr->disallow_perms.end(),
      "authorized_by appears in disallow_perms");
  }
}

void system::limitauthchg(const name & account, const std::vector<name> & allow_perms, const std::vector<name> & disallow_perms)
{
  _limitauthchg table(get_self(), get_self().value);
  require_auth(account);
  check(allow_perms.empty() || disallow_perms.empty(), "either allow_perms or disallow_perms must be empty");
  check(allow_perms.empty() || std::find(allow_perms.begin(), allow_perms.end(), name("owner")) != allow_perms.end(), "allow_perms does not contain owner");
  check(disallow_perms.empty() || std::find(disallow_perms.begin(), disallow_perms.end(), name("owner")) == disallow_perms.end(), "disallow_perms contains owner");

  auto itr = table.find(account.value);
  if(!allow_perms.empty() || !disallow_perms.empty()){
      if(itr == table.end()) {
        table.emplace(account, [&](auto& row){
          row.account = account;
          row.allow_perms = allow_perms;
          row.disallow_perms = disallow_perms;
        });
      } else {
        table.modify(itr, account, [&](auto& row){
          row.allow_perms = allow_perms;
          row.disallow_perms = disallow_perms;
        });
      }
  } else {
      if(itr != table.end()){
        table.erase(itr);
      }
  }
}

void system::newaccount(const name & creator, const name & new_account_name, ignore<authority> owner, ignore<authority> active)
{
  if(creator != get_self()) {
    uint64_t tmp = new_account_name.value >> 4;
    bool has_dot = false;

    for(uint32_t i = 0; i < 12; ++i){
      has_dot |= !(tmp & 0x1f);
      tmp >>= 5;
    }
    if(has_dot){
      auto suffix = new_account_name.suffix();
      if(suffix == new_account_name){
        _namebids bids(get_self(), get_self().value);
        auto current = bids.find( new_account_name.value);
        check(current != bids.end(), "no active bid for name");
        check(current->high_bidder == creator, "only highest bidder can claim");
        check(current->high_bid < 0, "auction for name is not closed yet");
        bids.erase(current);
      } else {
        check(creator == suffix, "only suffix may create this account");
      }
    }
  }

  _userres userres(get_self(), new_account_name.value);

  userres.emplace(new_account_name, [&](auto & row){
    row.owner = new_account_name;
    row.net_weight = asset(0, core_symbol);
    row.cpu_weight = asset(0, core_symbol);
  });
}

void system::setabi(const name & acnt, const std::vector<char> & abi, const binary_extension<std::string> & memo)
{
  multi_index<name("abihash"), _abihash_s> table(get_self(), get_self().value);
  auto itr = table.find( acnt.value );
  if( itr == table.end() ) {
    table.emplace( acnt, [&]( auto& row ) {
      row.owner = acnt;
      row.hash = eosio::sha256(const_cast<char*>(abi.data()), abi.size());
    });
  } else {
    table.modify( itr, same_payer, [&]( auto& row ) {
      row.hash = eosio::sha256(const_cast<char*>(abi.data()), abi.size());
    });
  }
}