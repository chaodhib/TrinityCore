CREATE TABLE `processed_shop_orders` 
(
  `id` INT(10) NOT NULL PRIMARY KEY,
  `ack_kafka_synced` BIT(1) NOT NULL DEFAULT 0
);

ALTER TABLE `characters` ADD COLUMN `kafka_synced` BIT(1) NOT NULL DEFAULT 0;
ALTER TABLE `characters` ADD COLUMN `gear_kafka_synced` BIT(1) NOT NULL DEFAULT 0;