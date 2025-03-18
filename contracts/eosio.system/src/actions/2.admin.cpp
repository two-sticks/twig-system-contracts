void systemcore::setwhitelist(const name & account, uint8_t depth)
{
  require_auth(get_self());
  _whitelist whitelist(get_self(), get_self().value);

  auto itr = whitelist.find(account.value);
  if (itr == whitelist.end() && depth > 0){
    whitelist.emplace(get_self(), [&](auto & row){
      row.account = account;
      row.depth = depth;
    });
  } else {
    if (depth > 0){
      whitelist.modify(itr, get_self(), [&](auto & row){
        row.depth = depth;
      });
    } else {
      whitelist.erase(itr);
    }
  }
}

void systemcore::setpriv(const name & account, uint8_t ispriv)
{
  require_auth(get_self());
  set_privileged(account, ispriv);
}

void systemcore::rmvproducer(const name & producer)
{
  require_auth(get_self());
  _producers producers(get_self(), get_self().value);
  auto prod = producers.find(producer.value);
  check(prod != producers.end(), "producer not found");
  producers.modify(prod, same_payer, [&](auto & row){
    row.deactivate();
  });
}

void systemcore::setrngcall(const uint64_t index, const name & contract, const name & action)
{
  require_auth(get_self());

  _rngcalls rngcalls(get_self(), get_self().value);
  auto rngcalls_itr = rngcalls.find(index);
  if (rngcalls_itr == rngcalls.end()){
    rngcalls.emplace(get_self(), [&](auto & row){
      row.index = index;
      row.contract = contract;
      row.action = action;
    });
  } else {
    rngcalls.modify(rngcalls_itr, get_self(), [&](auto & row){
      row.contract = contract;
      row.action = action;
      row.active = 0;
    });
  }
}
void systemcore::rmvrngcall(const uint64_t index)
{
  require_auth(get_self());

  _rngcalls rngcalls(get_self(), get_self().value);
  auto rngcalls_itr = rngcalls.find(index);
  if (rngcalls_itr != rngcalls.end()){
    rngcalls.erase(rngcalls_itr);
  }
}

void systemcore::modrngcall(const uint64_t index, const uint8_t active)
{
  require_auth(get_self());

  _rngcalls rngcalls(get_self(), get_self().value);
  auto rngcalls_itr = rngcalls.find(index);
  if (rngcalls_itr != rngcalls.end()){
    rngcalls.modify(rngcalls_itr, get_self(), [&](auto & row){
      row.active = active;
    });
  }
}