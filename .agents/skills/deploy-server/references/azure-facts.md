# Azure Facts

Name patterns, prices, billing states, the schedule, the budget, and cost
examples for each `/deploy-server` VM instance. The template under `../assets/`
is the source of every setting; this file never records the public IP.

## Names

`<instance>`: `SKILL.md` `## Inputs`.

| Item | Name |
| --- | --- |
| Region | `eastus` (the prices below are East US) |
| Resource group | `<instance>-server-rg` |
| VM | `<instance>-server` (guest computer name `gameserver`, admin user `beadmin`) |
| OS disk | `<instance>-server-osdisk` |
| Public IP | `<instance>-server-ip` |
| Network security group | `<instance>-server-nsg`, rules `Game-UDP-27015`, `Admin-RDP`, `Admin-SSH` |
| Virtual network | `<instance>-server-vnet`, subnet `default` |
| Network interface | `<instance>-server-nic` |
| Schedule workflow | `<instance>-server-schedule` |
| Budget-stop workflow | `<instance>-server-budget-stop` |
| Action group | `<instance>-server-budget-ag` |
| Budget | `<instance>-server-budget` |
| Deployment | `<instance>-server` (subscription scope) |

## Prices

East US list (retail) pay-as-you-go prices from the Azure Retail Prices API
(https://prices.azure.com/api/retail/prices, documented at
https://learn.microsoft.com/en-us/rest/api/cost-management/retail-prices/azure-retail-prices),
queried 2026-10-18. The calculator can differ by agreement.

- `Standard_D2s_v6` Windows: $0.193/hour while running; 2 vCPU and 8 GiB
  (https://learn.microsoft.com/en-us/azure/virtual-machines/sizes/general-purpose/dsv6-series).
- Standard SSD E10 (128 GiB): $9.60/month plus $0.002 per 10K transactions
  (https://learn.microsoft.com/en-us/azure/virtual-machines/disks-types).
- Standard static public IPv4: $0.005/hour, which is $3.65/month.
- Egress: the first 100 GB/month is free, then $0.087/GB
  (https://azure.microsoft.com/en-us/pricing/details/bandwidth/).
- Logic Apps Consumption built-in actions: the first 4,000 per month are free,
  then $0.000025 each; run-history data retention $0.12/GB-month.
- Action-group webhook notifications: the first 100,000 are free.
- Managed identities and role assignments carry no charge.

## Billing states

From https://learn.microsoft.com/en-us/azure/virtual-machines/states-billing:

- A deallocated VM is "Not billed" for compute, but "Disks and Networking
  continue to incur charges". The disk and the IP therefore bill $13.25/month
  whatever the VM state.
- "Stopped" is billed. That state results from the `PowerOff` API (`az vm stop`)
  or "invoking shutdown from within the guest OS". Stopping always means
  deallocating (https://learn.microsoft.com/en-us/cli/azure/vm#az-vm-deallocate).

## Schedule

The schedule workflow starts the VM at 08:00 and deallocates it at 23:00
`Eastern Standard Time` every day while the workflow is enabled. The recurrence
shifts its UTC time to follow daylight saving
(https://learn.microsoft.com/en-us/azure/logic-apps/concepts-schedule-automated-recurring-tasks-workflows
`### Trigger recurrence for daylight saving time and standard time`). A
re-enabled recurrence does not replay missed runs (same page,
`## Schedule triggers`). The schedule uses about 3 action executions per run,
2 runs a day, about 183 a month, inside the free 4,000.

## Budget

`<instance>-server-budget` is Cost, Monthly, $200, scoped to the instance's
resource group. Its one notification fires at 100% of actual cost: it emails the
signed-in account and calls the action group, which runs the budget-stop
workflow (`SKILL.md` `## Schedule and budget stop`). Budgets are evaluated about
every 24 hours on cost data that can lag 8-24 hours, so spend can pass $200
before the stop
(https://learn.microsoft.com/en-us/azure/cost-management-billing/costs/tutorial-acm-create-budgets).

The template omits `endDate`; Azure then defaults it to 10 years from the start
date (https://learn.microsoft.com/en-us/azure/templates/microsoft.consumption/budgets
`BudgetTimePeriod`, queried 2026-10-18).

## Cost examples

- On the daily 08:00-23:00 schedule: about 456.25 running hours a month
  (730 h × 15/24) × $0.193 = $88.06, plus $13.25 = about $101.31/month.
- Light use, 75 running hours a month: $14.48 compute + $13.25 = about
  $27.73/month.
- Running every hour of the month (730 h): $140.89 + $13.25 = about
  $154.14/month, so the $200 stop fires only when about $46 of other charges,
  mostly egress, add up.
- The schedule and budget automation: about $0.
- Disk transactions and egress beyond 100 GB are extra.

## Provisioned OS disk

Size and tier: none (recorded from `az disk show -g <instance>-server-rg -n <instance>-server-osdisk` after provisioning; the template fixes the image and disk type and leaves the size at the image default, so the record holds for every instance).
