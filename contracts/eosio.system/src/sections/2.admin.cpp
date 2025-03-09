void system::setwhitelist(const name & account, int8_t depth)
{
  require_auth(get_self());
  _whitelist whitelist(get_self(), get_self().value);

  auto itr = whitelist.find(account.value);
  if (itr == whitelist.end() && depth >= 1){
    whitelist.emplace(get_self(), [&](auto & row){
      row.account = account;
      row.depth = depth;
    });
  } else {
    if (depth >= 1){
      whitelist.modify(itr, get_self(), [&](auto & row){
        row.depth = depth;
      });
    } else {
      whitelist.erase(itr);
    }
  }
}

void system::setpriv(const name & account, uint8_t ispriv){
  require_auth(get_self());
  set_privileged(account, ispriv);
}

void system::rmvproducer(const name & producer){
  require_auth(get_self());
  auto prod = _producers.find(producer.value);
  check(prod != _producers.end(), "producer not found");
  _producers.modify(prod, same_payer, [&](auto& p){
    p.deactivate();
  });
}

void system::setacctram(const name & account){
  // require_auth(get_self()); commented out so that anyone can call it & fix their account if RAM is broken

  int64_t current_ram, current_net, current_cpu;
  get_resource_limits(account, current_ram, current_net, current_cpu);

  current_ram = user_ram_limit;

  set_resource_limits(account, current_ram, current_net, current_cpu);
}