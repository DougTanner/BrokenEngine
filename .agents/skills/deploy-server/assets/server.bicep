param instanceName string
param adminSourceAddress string
param alertEmail string

@secure()
param adminPassword string

param budgetStartDate string

var location = resourceGroup().location
var serverName = '${instanceName}-server'
var scheduleName = '${serverName}-schedule'
var vmApiVersion = '2026-04-01'
var workflowApiVersion = '2019-05-01'

// environment().resourceManager ends in '/' and every resource ID begins with '/'; drop one so each URL has a single '/'.
var resourceManager = take(environment().resourceManager, length(environment().resourceManager) - 1)

// resourceId() strings, not symbolic references: a symbolic VM reference closes the cycle
// VM -> NIC -> public IP -> budget -> action group -> budget-stop workflow -> VM.
var vmId = resourceId('Microsoft.Compute/virtualMachines', serverName)
var scheduleId = resourceId('Microsoft.Logic/workflows', scheduleName)

var managedIdentityAuthentication = {
	type: 'ManagedServiceIdentity'
}

var powerOnOffContributorRoleId = subscriptionResourceId('Microsoft.Authorization/roleDefinitions', '40c5ff49-9181-41f8-ae61-143b0e78555e')
var logicAppOperatorRoleId = subscriptionResourceId('Microsoft.Authorization/roleDefinitions', '515c2055-d9d4-4321-b1b9-bd0c9a0f79fe')

resource schedule 'Microsoft.Logic/workflows@2019-05-01' = {
	name: scheduleName
	location: location
	identity: {
		type: 'SystemAssigned'
	}
	properties: {
		state: 'Enabled'
		definition: {
			'$schema': 'https://schema.management.azure.com/providers/Microsoft.Logic/schemas/2016-06-01/workflowdefinition.json#'
			contentVersion: '1.0.0.0'
			triggers: {
				Recurrence: {
					type: 'Recurrence'
					recurrence: {
						frequency: 'Day'
						interval: 1
						// A fixed past start with no 'Z': no run fires at deployment, and the time zone applies.
						startTime: '2026-01-01T00:00:00'
						timeZone: 'Eastern Standard Time'
						schedule: {
							hours: [
								8
								23
							]
							minutes: [
								0
							]
						}
					}
				}
			}
			actions: {
				Start_or_deallocate: {
					type: 'If'
					expression: '@less(int(convertFromUtc(utcNow(), \'Eastern Standard Time\', \'HH\')), 12)'
					actions: {
						Start_vm: {
							type: 'Http'
							inputs: {
								method: 'POST'
								uri: '${resourceManager}${vmId}/start?api-version=${vmApiVersion}'
								authentication: managedIdentityAuthentication
							}
							operationOptions: 'DisableAsyncPattern'
						}
					}
					else: {
						actions: {
							Deallocate_vm: {
								type: 'Http'
								inputs: {
									method: 'POST'
									uri: '${resourceManager}${vmId}/deallocate?api-version=${vmApiVersion}'
									authentication: managedIdentityAuthentication
								}
								operationOptions: 'DisableAsyncPattern'
							}
						}
					}
					runAfter: {}
				}
			}
		}
	}
}

resource budgetStop 'Microsoft.Logic/workflows@2019-05-01' = {
	name: '${serverName}-budget-stop'
	location: location
	identity: {
		type: 'SystemAssigned'
	}
	properties: {
		state: 'Enabled'
		definition: {
			'$schema': 'https://schema.management.azure.com/providers/Microsoft.Logic/schemas/2016-06-01/workflowdefinition.json#'
			contentVersion: '1.0.0.0'
			triggers: {
				manual: {
					type: 'Request'
					kind: 'Http'
				}
			}
			actions: {
				Disable_schedule: {
					type: 'Http'
					inputs: {
						method: 'POST'
						uri: '${resourceManager}${scheduleId}/disable?api-version=${workflowApiVersion}'
						authentication: managedIdentityAuthentication
					}
					runAfter: {}
				}
				// Deallocate even when the disable fails: the deallocate is what stops the compute charge.
				Deallocate_vm: {
					type: 'Http'
					inputs: {
						method: 'POST'
						uri: '${resourceManager}${vmId}/deallocate?api-version=${vmApiVersion}'
						authentication: managedIdentityAuthentication
					}
					operationOptions: 'DisableAsyncPattern'
					runAfter: {
						Disable_schedule: [
							'Succeeded'
							'Failed'
						]
					}
				}
			}
		}
	}
}

resource budgetActionGroup 'Microsoft.Insights/actionGroups@2023-01-01' = {
	name: '${serverName}-budget-ag'
	location: 'global'
	properties: {
		groupShortName: 'bestop'
		enabled: true
		logicAppReceivers: [
			{
				name: 'budget-stop'
				resourceId: budgetStop.id
				callbackUrl: listCallbackUrl('${budgetStop.id}/triggers/manual', workflowApiVersion).value
				useCommonAlertSchema: false
			}
		]
	}
	dependsOn: [
		budgetStop
	]
}

resource schedulePowerOnOff 'Microsoft.Authorization/roleAssignments@2022-04-01' = {
	name: guid(resourceGroup().id, schedule.id, powerOnOffContributorRoleId)
	properties: {
		roleDefinitionId: powerOnOffContributorRoleId
		principalId: schedule.identity.principalId
		principalType: 'ServicePrincipal'
	}
}

resource budgetStopPowerOnOff 'Microsoft.Authorization/roleAssignments@2022-04-01' = {
	name: guid(resourceGroup().id, budgetStop.id, powerOnOffContributorRoleId)
	properties: {
		roleDefinitionId: powerOnOffContributorRoleId
		principalId: budgetStop.identity.principalId
		principalType: 'ServicePrincipal'
	}
}

resource budgetStopLogicAppOperator 'Microsoft.Authorization/roleAssignments@2022-04-01' = {
	name: guid(resourceGroup().id, budgetStop.id, logicAppOperatorRoleId)
	properties: {
		roleDefinitionId: logicAppOperatorRoleId
		principalId: budgetStop.identity.principalId
		principalType: 'ServicePrincipal'
	}
}

resource budget 'Microsoft.Consumption/budgets@2026-06-01' = {
	name: '${serverName}-budget'
	properties: {
		category: 'Cost'
		amount: 200
		timeGrain: 'Monthly'
		// No endDate, so the cap does not lapse.
		timePeriod: {
			startDate: budgetStartDate
		}
		notifications: {
			Actual_GreaterThanOrEqualTo_100_Percent: {
				enabled: true
				thresholdType: 'Actual'
				operator: 'GreaterThanOrEqualTo'
				threshold: 100
				contactEmails: [
					alertEmail
				]
				contactGroups: [
					budgetActionGroup.id
				]
			}
		}
	}
}

resource nsg 'Microsoft.Network/networkSecurityGroups@2025-09-01' = {
	name: '${serverName}-nsg'
	location: location
	properties: {
		securityRules: [
			{
				name: 'Game-UDP-27015'
				properties: {
					priority: 100
					direction: 'Inbound'
					access: 'Allow'
					protocol: 'Udp'
					sourceAddressPrefix: '*'
					sourcePortRange: '*'
					destinationAddressPrefix: '*'
					destinationPortRange: '27015'
				}
			}
			{
				name: 'Admin-RDP'
				properties: {
					priority: 110
					direction: 'Inbound'
					access: 'Allow'
					protocol: 'Tcp'
					sourceAddressPrefix: '${adminSourceAddress}/32'
					sourcePortRange: '*'
					destinationAddressPrefix: '*'
					destinationPortRange: '3389'
				}
			}
			{
				name: 'Admin-SSH'
				properties: {
					priority: 120
					direction: 'Inbound'
					access: 'Allow'
					protocol: 'Tcp'
					sourceAddressPrefix: '${adminSourceAddress}/32'
					sourcePortRange: '*'
					destinationAddressPrefix: '*'
					destinationPortRange: '22'
				}
			}
		]
	}
}

resource vnet 'Microsoft.Network/virtualNetworks@2025-09-01' = {
	name: '${serverName}-vnet'
	location: location
	properties: {
		addressSpace: {
			addressPrefixes: [
				'10.0.0.0/16'
			]
		}
		subnets: [
			{
				name: 'default'
				properties: {
					addressPrefix: '10.0.0.0/24'
				}
			}
		]
	}
}

// The first billed resource: it waits for the budget and the role assignments, so their failure bills nothing.
resource serverPublicIp 'Microsoft.Network/publicIPAddresses@2025-09-01' = {
	name: '${serverName}-ip'
	location: location
	sku: {
		name: 'Standard'
		tier: 'Regional'
	}
	properties: {
		publicIPAllocationMethod: 'Static'
		publicIPAddressVersion: 'IPv4'
	}
	dependsOn: [
		budget
		schedulePowerOnOff
		budgetStopPowerOnOff
		budgetStopLogicAppOperator
	]
}

resource nic 'Microsoft.Network/networkInterfaces@2025-09-01' = {
	name: '${serverName}-nic'
	location: location
	properties: {
		ipConfigurations: [
			{
				name: 'ipconfig1'
				properties: {
					privateIPAllocationMethod: 'Dynamic'
					privateIPAddressVersion: 'IPv4'
					subnet: {
						id: '${vnet.id}/subnets/default'
					}
					publicIPAddress: {
						id: serverPublicIp.id
					}
				}
			}
		]
		networkSecurityGroup: {
			id: nsg.id
		}
	}
}

resource vm 'Microsoft.Compute/virtualMachines@2026-04-01' = {
	name: serverName
	location: location
	properties: {
		hardwareProfile: {
			vmSize: 'Standard_D2s_v6'
		}
		storageProfile: {
			imageReference: {
				publisher: 'MicrosoftWindowsServer'
				offer: 'WindowsServer'
				sku: '2025-datacenter-azure-edition'
				version: 'latest'
			}
			osDisk: {
				name: '${serverName}-osdisk'
				createOption: 'FromImage'
				managedDisk: {
					storageAccountType: 'StandardSSD_LRS'
				}
			}
			diskControllerType: 'NVMe'
		}
		securityProfile: {
			securityType: 'TrustedLaunch'
			uefiSettings: {
				secureBootEnabled: true
				vTpmEnabled: true
			}
		}
		osProfile: {
			computerName: 'gameserver'
			adminUsername: 'beadmin'
			adminPassword: adminPassword
		}
		networkProfile: {
			networkInterfaces: [
				{
					id: nic.id
				}
			]
		}
	}
}

output publicIp string = serverPublicIp.properties.ipAddress
