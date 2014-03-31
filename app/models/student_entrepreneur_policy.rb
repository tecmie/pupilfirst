class StudentEntrepreneurPolicy < ActiveRecord::Base
  belongs_to :user
  mount_uploader :certificate_pic, AvatarUploader
  process_in_background :certificate_pic

  validates_presence_of :certificate_pic
  validates_presence_of :university_registration_number
  validates_presence_of :address
end
