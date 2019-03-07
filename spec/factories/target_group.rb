FactoryBot.define do
  factory :target_group do
    name { Faker::Lorem.words(6).join ' ' }
    sequence(:sort_index)
    description { Faker::Lorem.sentence }
    level
    milestone { false }

    trait :archived do
      safe_to_archive { true }
      archived { true }
    end
  end
end
