void systemcore::check_auth_change(name contract, name account, binary_extension<name> authorized_by)
{
  name authorized(authorized_by.has_value() ? authorized_by.value().value : 0);
  if(authorized.value){
    require_auth({account, authorized});
  }

  _limitauthchg limitauthchg(get_self(), get_self().value);
  auto limitauthchg_itr = limitauthchg.find(account.value);
  if(limitauthchg_itr == limitauthchg.end()){
    return;
  }
  check(authorized.value, "authorized_by is required for this account");

  if(!limitauthchg_itr->allow_perms.empty()){
    check(
      std::find(limitauthchg_itr->allow_perms.begin(), limitauthchg_itr->allow_perms.end(), authorized) != limitauthchg_itr->allow_perms.end(),
      "authorized_by does not appear in allow_perms");
  } else {
    check(
      std::find(limitauthchg_itr->disallow_perms.begin(), limitauthchg_itr->disallow_perms.end(), authorized) == limitauthchg_itr->disallow_perms.end(),
      "authorized_by appears in disallow_perms");
  }
}

void systemcore::limitauthchg(const name & account, const std::vector<name> & allow_perms, const std::vector<name> & disallow_perms)
{
  _limitauthchg limitauthchg(get_self(), get_self().value);
  require_auth(account);

  check(allow_perms.empty() || disallow_perms.empty(), "either allow_perms or disallow_perms must be empty");
  check(allow_perms.empty() || std::find(allow_perms.begin(), allow_perms.end(), name("owner")) != allow_perms.end(), "allow_perms does not contain owner");
  check(disallow_perms.empty() || std::find(disallow_perms.begin(), disallow_perms.end(), name("owner")) == disallow_perms.end(), "disallow_perms contains owner");

  auto limitauthchg_itr = limitauthchg.find(account.value);
  if(!allow_perms.empty() || !disallow_perms.empty()){
    if(limitauthchg_itr == limitauthchg.end()) {
      limitauthchg.emplace(account, [&](auto & row){
        row.account = account;
        row.allow_perms = allow_perms;
        row.disallow_perms = disallow_perms;
      });
    } else {
      limitauthchg.modify(limitauthchg_itr, account, [&](auto & row){
        row.allow_perms = allow_perms;
        row.disallow_perms = disallow_perms;
      });
    }
  } else {
    if(limitauthchg_itr != limitauthchg.end()){
      limitauthchg.erase(limitauthchg_itr);
    }
  }
}

void systemcore::newaccount(const name & creator, const name & new_account_name, ignore<authority> owner, ignore<authority> active)
{
  _whitelist whitelist(get_self(), get_self().value);
  auto whitelist_itr = whitelist.find(creator.value);

  if((creator != get_self()) || (whitelist_itr != whitelist.end() && whitelist_itr->depth >= 3)){
    uint64_t tmp = new_account_name.value >> 4;
    bool has_dot = false;

    for(uint32_t i = 0; i < 12; ++i){
      has_dot |= !(tmp & 0x1f);
      tmp >>= 5;
    }
    if(has_dot){
      auto suffix = new_account_name.suffix();
      if(suffix == new_account_name){
        check(false, "namebids currently disabled");
        /*
        _namebids namebids(names_account, names_account.value);
        auto namebids_itr = namebids.find(new_account_name.value);
        check(namebids_itr != namebids.end(), "no active bid for name");
        check(namebids_itr->high_bidder == creator, "only highest bidder can claim");
        check(namebids_itr->high_bid < 0, "auction for name is not closed yet");
        */
        eosio::action(permission_level{get_self(), name("active")}, names_account, name("cleanup"),
        std::make_tuple(new_account_name)).send();
      } else {
        check(creator == suffix, "only suffix may create this account");
      }
    }
  }

  /* Later ->
    Implement action for creating account objects within certain contracts
  */
}

void systemcore::setabi(const name & account, const std::vector<char> & abi, const binary_extension<std::string> & memo)
{
  _contractinfo contractinfo(get_self(), get_self().value);
  auto contractinfo_itr = contractinfo.find(account.value);

  if(contractinfo_itr == contractinfo.end()){
    contractinfo.emplace(account, [&](auto & row){
      row.owner = account;
      row.abi = temporal_256{.time = current_time_point(), .hash = eosio::sha256(const_cast<char*>(abi.data()), abi.size())};
    });
  } else {
    contractinfo.modify(contractinfo_itr, same_payer, [&](auto & row){
      row.abi = temporal_256{.time = current_time_point(), .hash = eosio::sha256(const_cast<char*>(abi.data()), abi.size())};
    });
  }
}

void systemcore::setcode(const name & account, uint8_t vmtype, uint8_t vmversion, const std::vector<char> & code, const binary_extension<std::string> & memo)
{
  _contractinfo contractinfo(get_self(), get_self().value);
  auto contractinfo_itr = contractinfo.find(account.value);

  if(contractinfo_itr == contractinfo.end()){
    contractinfo.emplace(account, [&](auto & row){
      row.owner = account;
      row.code = temporal_256{.time = current_time_point(), .hash = eosio::sha256(const_cast<char*>(code.data()), code.size())};
    });
  } else {
    contractinfo.modify(contractinfo_itr, same_payer, [&](auto & row){
      row.code = temporal_256{.time = current_time_point(), .hash = eosio::sha256(const_cast<char*>(code.data()), code.size())};
    });
  }
}

void systemcore::setcodeinfo(const name & account, const std::string & version, const std::string & source)
{
  require_auth(account);

  _contractinfo contractinfo(get_self(), get_self().value);
  auto contractinfo_itr = contractinfo.find(account.value);

  if(contractinfo_itr == contractinfo.end()){
    contractinfo.emplace(account, [&](auto & row){
      row.owner = account;
      row.version = version;
      row.source = source;
    });
  } else {
    contractinfo.modify(contractinfo_itr, same_payer, [&](auto & row){
      row.version = version;
      row.source = source;
    });
  }
}

void systemcore::cleanfix(const name & account)
{
  require_auth(get_self());
  multi_index<name("abihash"), _abihash_s> table(get_self(), get_self().value);
  auto itr = table.find(account.value);
  if(itr != table.end()){
    table.erase(itr);
  }

  _userres userres(get_self(), account.value);
  auto userres_itr = userres.find(account.value);
  if (userres_itr != userres.end()){
    userres.erase(userres_itr);
  }
}