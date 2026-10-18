using '../../../.agents/skills/deploy-server/assets/main.bicep'

param instanceName = 'brokenengine'
param adminSourceAddress = readEnvironmentVariable('BROKENENGINE_ADMIN_SOURCE_ADDRESS')
param alertEmail = readEnvironmentVariable('BROKENENGINE_ALERT_EMAIL')
param adminPassword = readEnvironmentVariable('BROKENENGINE_ADMIN_PASSWORD')
