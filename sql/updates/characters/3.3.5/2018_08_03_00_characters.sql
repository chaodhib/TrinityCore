CREATE TABLE `processed_shop_orders` (
  `id` INT(10) UNSIGNED NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE INDEX `id_UNIQUE` (`id` ASC));

ALTER TABLE `characters` ADD COLUMN `kafka_synced` BIT(1) NOT NULL DEFAULT 0;
ALTER TABLE `characters` ADD COLUMN `gear_kafka_synced` BIT(1) NOT NULL DEFAULT 0;
ALTER TABLE `processed_shop_orders` ADD COLUMN `ack_kafka_synced` BIT(1) NOT NULL DEFAULT 0;