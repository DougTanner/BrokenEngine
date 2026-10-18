targetScope = 'subscription'

@maxLength(24)
@description('The instance name every Azure name starts with: ASCII letters, digits, and hyphens, starting with a letter or digit, at most 24 characters.')
param instanceName string

@description('The user\'s public IPv4 address, without a prefix length; RDP and SSH admit only this /32.')
param adminSourceAddress string

@description('Budget notification address: the signed-in account\'s user.name.')
param alertEmail string

@secure()
@minLength(12)
@description('VM administrator password, supplied by main.bicepparam from an environment variable.')
param adminPassword string

@description('First day of the current UTC month, the budget start.')
param budgetStartDate string = utcNow('yyyy-MM-01T00:00:00Z')

resource serverResourceGroup 'Microsoft.Resources/resourceGroups@2025-04-01' = {
	name: '${instanceName}-server-rg'
	location: 'eastus'
}

module server 'server.bicep' = {
	name: '${instanceName}-server-resources'
	scope: serverResourceGroup
	params: {
		instanceName: instanceName
		adminSourceAddress: adminSourceAddress
		alertEmail: alertEmail
		adminPassword: adminPassword
		budgetStartDate: budgetStartDate
	}
}

output publicIp string = server.outputs.publicIp
