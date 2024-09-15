use clap;

use serde_json::json;
use std::{collections::HashMap, error::Error, fs::File, io::Write};

use serde::{Deserialize, Serialize};

fn load_translation_json(filename: String) -> Result<HashMap<String, String>, Box<dyn Error>> {
    let f = std::fs::File::open(filename).unwrap();
    let tmp: Vec<HashMap<String, String>> = serde_json::from_reader(f)?;

    let mut result = HashMap::new();

    for t in tmp {
        for (key, value) in t {
            result.insert(key, value);
        }
    }
    Ok(result)
}

fn write_to_file(filename: &str, data: &serde_yaml::Value) -> Result<(), Box<dyn Error>> {
    // Convert the data back to a YAML string
    let yaml_string = serde_yaml::to_string(&data)?;

    // Open a file to write
    let mut file = File::create(filename)?;

    // Write the YAML string to the file
    file.write_all(yaml_string.as_bytes())?;

    Ok(())
}

fn main() {
    let translation_table = load_translation_json("german_to_english.json".to_string()).unwrap();
    let f = std::fs::File::open("heatingpump.yaml").unwrap();
    let mut data: serde_yaml::Value = serde_yaml::from_reader(f).unwrap();

    // Sensor
    //dbg!(&data["sensor"]);
    if let serde_yaml::Value::Sequence(sensor_array) = &mut data["sensor"] {
        for s in sensor_array {
            // dbg!(&s["name"]);
            if let Some(s) = s.get_mut("name") {
                let tmp = s.as_str().unwrap().to_string();
                println!("{}:{}", tmp, translation_table.get(&tmp).unwrap());
                *s = serde_yaml::to_value(translation_table.get(&tmp).unwrap().clone()).unwrap();
            }
        }
    }

    // button -> name
    //dbg!(&data["button"]);
    if let serde_yaml::Value::Sequence(sensor_array) = &mut data["button"] {
        for s in sensor_array {
            //dbg!(&s["name"]);
            if let Some(s) = s.get_mut("name") {
                let tmp = s.as_str().unwrap().to_string();
                println!("{}:{}", tmp, translation_table.get(&tmp).unwrap());
                *s = serde_yaml::to_value(translation_table.get(&tmp).unwrap().clone()).unwrap();
            }
        }
    }
    // text_sensor -> name
    //dbg!(&data["text_sensor"]);
    if let serde_yaml::Value::Sequence(sensor_array) = &mut data["text_sensor"] {
        for s in sensor_array {
            //dbg!(&s["name"]);
            if let Some(s) = s.get_mut("name") {
                let tmp = s.as_str().unwrap().to_string();
                println!("{}:{}", tmp, translation_table.get(&tmp).unwrap());
                *s = serde_yaml::to_value(translation_table.get(&tmp).unwrap().clone()).unwrap();
            }
        }
    }

    write_to_file("heatingpump_en.yaml", &data);
}
